#ifndef GENETIC_ALGORITHM_H
#define GENETIC_ALGORITHM_H

#include "../10_common/Instance.h"
#include "../10_common/Types.h"
#include "../10_common/Solution.h"
#include "../10_common/Config.h"
#include "../10_common/Random.h"
#include "../3_monitor/Monitor.h"
#include "../4_logger/Logger.h"
#include "../10_common/ThreadPool.h"
#include "../7_formula/08_Objective.h"
#include "../7_formula/05_InvestmentCost.h"
#include "04_BranchAndPrice.h"
#include <vector>
#include <mutex>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <functional>
#include <iomanip>
#include <sstream>
#include <future>
#include <array>
#include <mutex>
// fork 并行评估所需头文件
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cstring>
#include <sstream>

/**
 * 遗传算法主框架（Genetic Algorithm）
 *
 * 对应论文第4层：外层遗传算法对减排率 w_j 编码
 * 日志层级：Level 0 [GA]，缩进0空格
 *
 * 算法流程：
 *   1. 初始化种群（随机生成 w_j 的二进制编码）
 *   2. 对每个个体评估适应度（调用分支定价）
 *   3. 选择（轮盘赌）→ 交叉（两点交叉，概率0.7）→ 变异（概率0.2）
 *   4. 精英保留
 *   5. 迭代直到最大代数
 *   6. 返回最优 w_j 和对应的最优解
 */
class GeneticAlgorithm {
private:
    const Config* config;
    Instance* inst;
    Random rng;
    Logger* logger;
    Monitor* monitor;

    // 分支定价求解器
    BranchAndPrice bp;

    // 当前种群
    std::vector<std::vector<double>> population;

    // 收敛曲线数据
    std::vector<ConvergencePoint> convergence;

    // 最优解
    double bestFitness;
    std::vector<double> bestW;
    Solution bestSolution;

    // 时间统计
    std::chrono::steady_clock::time_point startTime;

    // 累计各阶段耗时（供上报使用）
    double accumulatedSelectTime_ = 0.0;
    double accumulatedCrossoverTime_ = 0.0;
    double accumulatedMutateTime_ = 0.0;
    double accumulatedDecodeTime_ = 0.0;
    double accumulatedEvalTime_ = 0.0;

    // 并行适应度评估累计统计（由子进程回传后在父进程聚合）
    double parallelBpTime_ = 0.0;
    double parallelCgTime_ = 0.0;
    double parallelPsTime_ = 0.0;
    double parallelBbTime_ = 0.0;
    double parallelCplexBuildTime_ = 0.0;
    double parallelCplexSolveTime_ = 0.0;
    int parallelCgIterations_ = 0;
    int parallelCgColumns_ = 0;
    int parallelTotalNodes_ = 0;
    int parallelPrunedNodes_ = 0;

    // 串行适应度评估累计统计：与并行 fork 完全对称——逐个体取「本次求解增量」累加，
    // 缓存命中不计入。这样串行/并行上报给 monitor 的 CG 迭代口径完全一致，
    // 都表示「所有真实求解的 CG 迭代总和」，可直接横向对比。
    int serialCgIterations_ = 0;
    int serialCgColumns_ = 0;
    int serialTotalNodes_ = 0;
    int serialPrunedNodes_ = 0;

    // 并行管理开销累计（父进程侧墙钟计时，用于诊断 fork 并行是否划算）
    double parallelForkTime_ = 0.0;    // fork() + pipe() 创建子进程的墙钟耗时
    double parallelWaitTime_ = 0.0;    // waitpid 等待子进程结束的墙钟耗时
    double parallelMergeTime_ = 0.0;   // readChild 读管道 + 聚合结果的墙钟耗时
    double parallelWallTime_ = 0.0;    // 每代 fork 段整体墙钟（含调度），跨代累加
    double serialEquivBpTime_ = 0.0;   // 各子进程 BP 耗时之和（= 串行等效计算时间）
    int parallelGenerations_ = 0;      // 实际走并行的代数

    // 「最近一代」并行流水快照（供 run() 每代打印，非累计）
    double lastGenWall_ = 0.0;
    double lastGenSerialEquiv_ = 0.0;
    double lastGenForkTime_ = 0.0;
    double lastGenWaitTime_ = 0.0;
    double lastGenMergeTime_ = 0.0;
    int lastGenCgIterations_ = 0;
    int lastGenCgColumns_ = 0;
    int lastGenTotalNodes_ = 0;
    bool lastGenParallel_ = false;

public:
    GeneticAlgorithm(const Config& cfg, Instance& instance)
        : config(&cfg), inst(&instance), rng(cfg.random_seed), logger(nullptr),
          monitor(nullptr), bestFitness(-DBL_MAX) {

        bp.setInstance(instance);
        bp.setConfig(cfg);
        startTime = std::chrono::steady_clock::now();
    }

    void setLogger(Logger* log) {
        logger = log;
        bp.setLogger(log);
    }

    void setMonitor(Monitor* mon) {
        monitor = mon;
        bp.setMonitor(mon);
    }
    Monitor* getMonitor() { return monitor; }
    const std::vector<ConvergencePoint>& getConvergence() const { return convergence; }
    double getBestFitness() const { return bestFitness; }
    const std::vector<double>& getBestW() const { return bestW; }
    const Solution& getBestSolution() const { return bestSolution; }

    /** 获取 BP 内部的时间统计 */
    double getBPTime() const { return bp.getBPTime(); }
    double getCGTime() const { return bp.getCGTime(); }
    double getPricingTime() const { return bp.getPricingTime(); }
    double getBBTime() const { return bp.getBBTime(); }

    /**
     * 运行遗传算法
     */
    Solution run() {
        int popSize = config->population_size;
        int maxGen = config->max_generation;
        int chromLen = config->chromosome_length;
        int geneLength = chromLen * inst->numDC;
        double effectiveMutationRate =
            config->mutation_rate > 0.0
                ? config->mutation_rate
                : 1.0 / static_cast<double>(geneLength);

        bestFitness = -DBL_MAX;
        bestW.resize(inst->numDC, 0.0);

        // ===== 1. 初始化种群 =====
        population = initializePopulation(popSize, geneLength);

        // ===== 环节配置横幅：GA 层（第4层，外层元启发式） =====
        std::cout << "\n========== [环节1/5] 遗传算法 GA（外层：对减排率 w_j 编码搜索） ==========" << std::endl;
        std::cout << "  设计: 二进制编码 w_j → 轮盘赌选择 → 两点交叉 → 按位变异 → 精英保留" << std::endl;
        std::cout << "  配置: 种群=" << popSize << " | 最大代数=" << maxGen
                  << " | 编码位数=" << chromLen << "/DC | 基因总长=" << geneLength << std::endl;
        std::cout << "        交叉率=" << config->crossover_rate
                  << " | 逐位变异率=" << effectiveMutationRate
                  << (config->mutation_rate > 0.0 ? "(固定)" : "(自适应1/L)")
                  << " | 精英保留=" << (config->elitism ? "是" : "否")
                  << " | w范围=[" << config->min_w << "," << config->max_w << "]" << std::endl;
        std::cout << "        提前停止=" << (config->early_stop ? "是" : "否")
                  << " | 连续未提升阈值=" << config->convergence_generations << "代"
                  << " | 提升容差=" << config->convergence_tolerance << std::endl;
        std::cout << "  优化: 二进制解码查表(替代 std::pow) | 每 w 命中 BP 层 fitnessCache 直接返回" << std::endl;

        // ===== 2. 主迭代 =====
        int noImproveCount = 0;
        double prevBest = -DBL_MAX;

        for (int gen = 0; gen < maxGen; gen++) {
            auto genStart = std::chrono::steady_clock::now();

            // 2a. 计算适应度
            std::vector<double> fitness = calculateFitness(population, gen);

            // 2b. 统计
            double genBest = fitness[0];
            double genWorst = fitness[0];
            double genSum = 0.0;
            for (size_t i = 0; i < fitness.size(); i++) {
                genSum += fitness[i];
                if (fitness[i] > genBest) genBest = fitness[i];
                if (fitness[i] < genWorst) genWorst = fitness[i];
            }
            double genAvg = genSum / fitness.size();

            // 2c. 更新全局最优
            int bestIdx = -1;
            for (size_t i = 0; i < fitness.size(); i++) {
                if (fitness[i] > bestFitness) {
                    bestFitness = fitness[i];
                    bestIdx = (int)i;
                }
            }

            if (bestIdx >= 0) {
                for (int j = 0; j < inst->numDC; j++) {
                    std::vector<double> v(population[bestIdx].begin() + j * chromLen,
                                          population[bestIdx].begin() + (j + 1) * chromLen);
                    bestW[j] = binaryToDecimal(v, config->min_w, config->max_w, chromLen);
                }
            }

            // 2d. 提前停止检测
            bool improved = false;
            if (config->early_stop && gen > 0) {
                double relImprove = (bestFitness - prevBest) / std::max(1.0, std::abs(prevBest));
                if (relImprove > config->convergence_tolerance) {
                    noImproveCount = 0;
                    improved = true;
                } else {
                    noImproveCount++;
                }
            }
            prevBest = bestFitness;

            // 2e. 记录收敛曲线
            double elapsed = getElapsedSeconds();
            convergence.push_back({gen, bestFitness, genAvg, genWorst, elapsed});

            // 2f. 输出摘要（每代一行）
            auto genEnd = std::chrono::steady_clock::now();
            double genTime = std::chrono::duration<double>(genEnd - genStart).count();

            std::ostringstream summary;
            summary << std::fixed << std::setprecision(2)
                    << "代数 " << gen << ": Best=" << bestFitness
                    << ", Avg=" << genAvg
                    << ", Worst=" << genWorst
                    << ", 耗时=" << genTime << "s";

            if (config->early_stop && noImproveCount > 0) {
                summary << " (连续" << noImproveCount << "代未提升";
                if (noImproveCount >= config->convergence_generations) {
                    summary << " → 提前停止";
                }
                summary << ")";
            }
            // progress: 既进 run.log 文件又输出终端，便于事后回看每代过程
            if (logger) {
                logger->progress(0, summary.str());
            } else {
                std::cout << summary.str() << std::endl;
            }

            // 2f-bis. 每代求解流水（展示求解思路 + 并行/串行实况）
            {
                std::ostringstream pipe;
                pipe << std::fixed << std::setprecision(4);
                if (lastGenParallel_) {
                    double wall = lastGenWall_;
                    double serialEquiv = lastGenSerialEquiv_;
                    double mgmt = wall - serialEquiv;  // 并行净管理开销
                    double speedup = wall > 1e-9 ? serialEquiv / wall : 0.0;
                    pipe << "  ├─ 求解: " << (int)population.size()
                         << " 个体 fork 并行 | CG迭代=" << lastGenCgIterations_
                         << " 加列=" << lastGenCgColumns_
                         << " BB节点=" << lastGenTotalNodes_ << "\n";
           pipe << "  ├─ 并行墙钟=" << wall << "s | 串行等效(ΣBP)="
                         << serialEquiv << "s | 加速比="
                         << std::setprecision(2) << speedup << "x"
                         << std::setprecision(4) << "\n";
                    pipe << "  ├─ 管理开销=" << mgmt << "s (fork="
                         << lastGenForkTime_ << " wait=" << lastGenWaitTime_
                         << " merge=" << lastGenMergeTime_ << ")";
                    if (speedup < 1.0) {
                        pipe << "  [注意: 加速比<1，并行反而更慢，此规模建议串行]";
                    }
                    // 并行时每个个体的 CG 迭代明细写在各自独立日志文件里
                    pipe << "\n  └─ CG明细: gen_logs/run_gen" << gen
                         << "_ind{0.." << ((int)population.size() - 1) << "}.log";
                } else {
                    pipe << "  └─ 求解: 个体串行顺序评估"
                         << " (fitnessCache 可命中，无 fork 开销)";
                }
                if (logger) {
                    logger->raw(pipe.str());
                } else {
                    std::cout << pipe.str() << std::endl;
                }
            }

            // 提前停止
            if (config->early_stop && noImproveCount >= config->convergence_generations) {
                break;
            }

            // 2g. 遗传操作（最后一代不进行）
            if (gen < maxGen - 1) {

                // 选择（轮盘赌）
                auto selStart = std::chrono::steady_clock::now();
                std::vector<double> realFitness = fitness;
                std::vector<std::vector<double>> parents = selectParents(population, realFitness);
                auto selEnd = std::chrono::steady_clock::now();
                accumulatedSelectTime_ += std::chrono::duration<double>(selEnd - selStart).count();

                // 交叉（两点交叉）
                auto crossStart = std::chrono::steady_clock::now();
                std::vector<std::vector<double>> offspring = crossover(parents);
                auto crossEnd = std::chrono::steady_clock::now();
                accumulatedCrossoverTime_ += std::chrono::duration<double>(crossEnd - crossStart).count();

                // 变异（按位翻转）
                auto mutStart = std::chrono::steady_clock::now();
                mutate(offspring);
                auto mutEnd = std::chrono::steady_clock::now();
                accumulatedMutateTime_ += std::chrono::duration<double>(mutEnd - mutStart).count();

                population = replacePopulation(population, fitness, offspring);
            }
        }

        // ===== 3. 输出最终结果 =====
        auto end = std::chrono::steady_clock::now();
        double totalTime = std::chrono::duration<double>(end - startTime).count();

        std::cout << "\n========== 遗传算法完成 ==========" << std::endl;
        std::cout << "总耗时: " << totalTime << "s" << std::endl;
        std::cout << "最优适应度: " << std::fixed << std::setprecision(2) << bestFitness << std::endl;
        std::cout << "最优w: ";
        for (int j = 0; j < inst->numDC; j++) {
            std::cout << "w" << j << "=" << bestW[j] << " ";
        }
        std::cout << std::endl;

        // ===== 4. 上报时间统计到 Monitor =====
        if (monitor) {
            monitor->reportGATiming(totalTime,
                                     accumulatedSelectTime_,
                                     accumulatedCrossoverTime_,
                                     accumulatedMutateTime_,
                                     accumulatedDecodeTime_,
                                     accumulatedEvalTime_);
            if (config->parallel_fitness && config->num_threads != 1) {
                // 并行模式下 BP 采用墙钟口径：与 GA 适应度评估耗时一致，避免累加各子进程耗时后超过总时间
                monitor->reportBPTiming(accumulatedEvalTime_);
                monitor->reportCGTiming(parallelCgTime_,
                                         parallelCplexBuildTime_,
                                         parallelCplexSolveTime_);
                monitor->reportPricingTiming(parallelPsTime_);
                monitor->reportBBTiming(parallelBbTime_);
                monitor->reportBBNodes(parallelTotalNodes_, parallelPrunedNodes_);
                monitor->reportCGIteration(parallelCgIterations_, parallelCgColumns_);
                // 上报并行开销分项，使诊断树不再是全 0
                // pipeIo 未单独计量（含在 fork 里），传 0
                monitor->reportBPParallelOverhead(parallelForkTime_, 0.0,
                                                  parallelWaitTime_, parallelMergeTime_);
            } else {
                // 串行模式：所有 BP 调用都通过主 bp 对象，各时间累加正确
                monitor->reportBPTiming(bp.getBPTime());
                monitor->reportCGTiming(bp.getCGTime(),
                                         bp.getCplexBuildTime(),
                                         bp.getCplexSolveTime());
                monitor->reportPricingTiming(bp.getPricingTime());
                monitor->reportBBTiming(bp.getBBTime());
                // 串行 CG 迭代/节点采用 serial* 累加器（逐个体增量、缓存命中不计），
                // 与并行 reportBBNodes/reportCGIteration 口径完全对称
                monitor->reportBBNodes(serialTotalNodes_, serialPrunedNodes_);
                monitor->reportCGIteration(serialCgIterations_, serialCgColumns_);
            }
        }

        // ===== 4b. 并行效率总报告（是否划算） =====
        if (config->parallel_fitness && config->num_threads != 1
            && parallelGenerations_ > 0) {
            double wall = parallelWallTime_;
            double serialEquiv = serialEquivBpTime_;
            double speedup = wall > 1e-9 ? serialEquiv / wall : 0.0;
            double mgmt = wall - serialEquiv;  // 净管理开销
            double mgmtPct = wall > 1e-9 ? mgmt / wall * 100.0 : 0.0;

            std::ostringstream rep;
            rep << std::fixed << std::setprecision(4);
            rep << "\n========== 并行效率总报告 ==========\n";
            rep << "并行代数            : " << parallelGenerations_ << "\n";
            rep << "并行墙钟(累计)      : " << wall << "s\n";
            rep << "串行等效(ΣBP累计)   : " << serialEquiv << "s\n";
            rep << "净管理开销          : " << mgmt << "s ("
                << std::setprecision(1) << mgmtPct << "%)"
                << std::setprecision(4) << "\n";
            rep << "  ├─ fork  : " << parallelForkTime_ << "s\n";
            rep << "  ├─ wait  : " << parallelWaitTime_ << "s\n";
            rep << "  └─ merge : " << parallelMergeTime_ << "s\n";
            rep << "加速比              : " << std::setprecision(2)
                << speedup << "x" << std::setprecision(4) << "\n";
            if (speedup < 1.0) {
                rep << "结论: 加速比 < 1，并行反而更慢（负优化）。"
                    << "单次 BP 求解太快，fork/进程管理开销占主导，"
                    << "该规模建议设 parallel_fitness=false。\n";
            } else {
                rep << "结论: 加速比 > 1，并行有效收益。\n";
            }
            rep << "====================================";
            if (logger) {
                logger->raw(rep.str());
            } else {
                std::cout << rep.str() << std::endl;
            }
        }

        // ===== 5. 用最优w重新求解，输出详细结果 =====
        std::cout << "\n--- 最优解详细结果 ---" << std::endl;
        bestSolution = solveWithDetailedResult(bestW);

        return bestSolution;
    }

private:
    std::vector<std::vector<double>> initializePopulation(int popSize, int geneLength) {
        std::vector<std::vector<double>> pop(popSize, std::vector<double>(geneLength));
        // 第一个个体：w_j 全为0（论文4.5.1：初始种群特殊个体）
        for (int j = 0; j < geneLength; j++) {
            pop[0][j] = 0;
        }
        // 其余个体随机生成
        for (int i = 1; i < popSize; i++) {
            for (int j = 0; j < geneLength; j++) {
                pop[i][j] = rng.bernoulli(0.5);
            }
        }
        return pop;
    }

    /**
     * 二进制解码（查表优化版）
     *
     * 用预计算的 pow2 表和 maxVal 替代 std::pow 调用。
     * chromLen 最大为 10（论文参数），查表足够覆盖。
     *
     * 优化前：每代对每个个体每个DC调用 pow(2.0, k) 多次
     * 优化后：一次查表
     */
    double binaryToDecimal(const std::vector<double>& binary,
                            double minVal, double maxVal, int chromLen) {
        // 静态局部变量：只计算一次
        static double pow2Table[11];      // 2^0 ~ 2^10
        static double maxValTable[11];    // (2^chromLen - 1)
        static bool initialized = false;
        if (!initialized) {
            for (int k = 0; k <= 10; k++) {
                pow2Table[k] = std::pow(2.0, k);
                maxValTable[k] = pow2Table[k] - 1.0;
            }
            initialized = true;
        }

        double decimal = 0.0;
        for (int i = 0; i < chromLen; i++) {
            decimal += binary[i] * pow2Table[chromLen - 1 - i];
        }
        return minVal + decimal / maxValTable[chromLen] * (maxVal - minVal);
    }

    std::string wToString(const std::vector<double>& w) {
        std::string s;
        for (size_t i = 0; i < w.size(); i++) {
            if (i > 0) s += ", ";
            s += std::to_string(w[i]);
        }
        return s;
    }

    /**
     * 计算种群适应度（支持并行）
     */
    std::vector<double> calculateFitness(const std::vector<std::vector<double>>& pop,
                                         int gen = -1) {
        int popSize = pop.size();
        std::vector<double> fitness(popSize, -DBL_MAX);

        bool parallel = config->parallel_fitness;
        int numThreads = config->num_threads;
        if (numThreads <= 0) numThreads = std::thread::hardware_concurrency();
        if (numThreads < 1) numThreads = 1;

        // 先把所有 w 向量算好（计时：解码操作）
        auto decodeStart = std::chrono::steady_clock::now();
        std::vector<std::vector<double>> all_w(popSize);
        for (int i = 0; i < popSize; i++) {
            all_w[i].resize(inst->numDC);
            for (int j = 0; j < inst->numDC; j++) {
                std::vector<double> v(pop[i].begin() + j * config->chromosome_length,
                                      pop[i].begin() + (j + 1) * config->chromosome_length);
                all_w[i][j] = binaryToDecimal(v, config->min_w, config->max_w, config->chromosome_length);
            }
        }
        auto decodeEnd = std::chrono::steady_clock::now();
        accumulatedDecodeTime_ += std::chrono::duration<double>(decodeEnd - decodeStart).count();

        // ===== 结构化并行诊断横幅（只打印一次） =====
        // 目的：说清"哪一层并行 / 并发多少 / 共享还是深拷贝 / 下层求解器状态"，
        //       并逐条打印并行三条件的实际取值，便于快速判断为何走并行或串行。
        static bool evalBannerPrinted = false;
        if (!evalBannerPrinted) {
            bool condParallelFlag = parallel;
            bool condPop = (popSize > 1);
            bool condThreads = (numThreads > 1);
            bool willParallel = (condParallelFlag && condPop && condThreads);
            unsigned hwCores = std::thread::hardware_concurrency();

            std::cout << "\n  +--------------- [并行诊断] 适应度评估层（GA 第4层外层） ---------------" << std::endl;
            std::cout << "  | 评估任务: 每代对 " << popSize
                      << " 个个体各调用一次分支定价 BP.solve(w)（含 CG->定价->BB）" << std::endl;
            std::cout << "  | 并行三条件:" << std::endl;
            std::cout << "  |   1) parallel_fitness = " << (condParallelFlag ? "true" : "false")
                      << "   [" << (condParallelFlag ? "满足" : "不满足") << "]" << std::endl;
            std::cout << "  |   2) population_size > 1 : " << popSize
                      << "   [" << (condPop ? "满足" : "不满足") << "]" << std::endl;
            std::cout << "  |   3) num_threads > 1 : 配置=" << config->num_threads
                      << " -> 实际=" << numThreads
                      << "（0 表示自动取 CPU 核数=" << hwCores << "）"
                      << "   [" << (condThreads ? "满足" : "不满足") << "]" << std::endl;
            std::cout << "  +---------------------------------------------------------------" << std::endl;

            if (willParallel) {
                std::cout << "  | >>> 当前模式: 【并行】GA 适应度评估层 fork 进程级并行" << std::endl;
                std::cout << "  |     并发度: " << numThreads << " 个子进程同时运行"
                          << "（个体总数 " << popSize << "，分批调度）" << std::endl;
                std::cout << "  |     隔离方式: 每个体 fork() 独立子进程 + Instance.deepCopy() 深拷贝" << std::endl;
                std::cout << "  |               （不共享求解器，各持独立 BranchAndPrice，无锁无竞争）" << std::endl;
                std::cout << "  |     结果回传: 子进程算完经管道 pipe() 回传 fitness 及 BP/CG/PS 计时" << std::endl;
                std::cout << "  |     下层状态: 每个子进程内部 BP->CG->定价PS->BB 全部【串行】" << std::endl;
                std::cout << "  |     计时口径: 并行时 BP/CG/PS 取墙钟聚合，避免子进程 CPU 时间累加超过总时间" << std::endl;
            } else {
                std::cout << "  | >>> 当前模式: 【串行】逐个个体顺序评估" << std::endl;
                std::cout << "  |     执行方式: 复用同一 BranchAndPrice 对象，个体间 fitnessCache 可命中" << std::endl;
                std::cout << "  |     未并行原因: ";
       if (!condParallelFlag) std::cout << "parallel_fitness=false ";
                if (!condPop)          std::cout << "种群规模<=1 ";
                if (!condThreads)      std::cout << "可用线程数<=1 ";
                std::cout << std::endl;
                std::cout << "  |     下层状态: BP->CG->定价PS->BB 全部串行" << std::endl;
            }
            std::cout << "  | 注: 定价子问题 PS 并行开关 parallel_pricing="
                      << (config->parallel_pricing ? "true" : "false")
                      << " —— true 时并行逐DC，false 时串行逐DC；线程数配置在 CG 环节横幅始终展示，仅 false 时不生效）" << std::endl;
            std::cout << "  +---------------------------------------------------------------" << std::endl;
            evalBannerPrinted = true;
        }

        // 计时：BP 求解（适应度评估）
        auto evalStart = std::chrono::steady_clock::now();

        if (parallel && popSize > 1 && numThreads > 1) {
            // 从 config->log_file（run.log 完整路径）派生子进程日志目录
            // 每个子进程写独立文件 gen_logs/run_gen{代}_ind{个体}.log，
            // 避免多进程同写 run.log 造成交错乱序（fork 后 logMutex 失效）
            std::string childLogDir;
            {
                std::string logPath = config->log_file;
                std::string baseDir;
                size_t slash = logPath.find_last_of("/\\");
                if (slash != std::string::npos) {
                    baseDir = logPath.substr(0, slash);
                } else {
                    baseDir = ".";
                }
                childLogDir = baseDir + "/gen_logs";
            }

            struct ChildResult {
                double fitness;
                double bpTime;
                double cgTime;
                double psTime;
                double bbTime;
                double cplexBuildTime;
                double cplexSolveTime;
                int cgIterations;
                int cgColumns;
                int totalNodes;
                int prunedNodes;
            };

            std::vector<int> childPids(popSize, -1);
            std::vector<std::array<int, 2>> pipes(popSize);
            int running = 0;
            int nextIdx = 0;
            int targetCompleted = popSize;

            double genBpTime = 0.0;
            double genCgTime = 0.0;
            double genPsTime = 0.0;
            double genBbTime = 0.0;
            double genCplexBuildTime = 0.0;
            double genCplexSolveTime = 0.0;
            int genCgIterations = 0;
            int genCgColumns = 0;
            int genTotalNodes = 0;
            int genPrunedNodes = 0;

            // 本代并行管理开销计时（墙钟口径，用于量化 fork 并行的额外成本）
            double genForkTime = 0.0;   // pipe()+fork() 创建子进程
            double genWaitTime = 0.0;   // waitpid 等待
            double genMergeTime = 0.0;  // readChild 读管道+聚合
            auto genWallStart = std::chrono::steady_clock::now();

            auto launchChild = [&](int idx) -> bool {
                auto forkStart = std::chrono::steady_clock::now();
                if (pipe(pipes[idx].data()) == -1) return false;
                pid_t pid = fork();
                if (pid == -1) {
                    close(pipes[idx][0]);
                    close(pipes[idx][1]);
                    return false;
                }
                if (pid == 0) {
                    close(pipes[idx][0]);
                    Instance localInst = inst->deepCopy();
                    BranchAndPrice localBP;
                    localBP.setInstance(localInst);
                    // 防死锁：GA fork 并行时每个子进程已独占一个 CPU 核，
                    // 若此时再让子进程内的 CPLEX(RMP) 开多线程，会造成
                    // popSize 个子进程 × cplex_threads 的线程过度订阅（oversubscription），
                    // 叠加 CPLEX 全局 license 锁极易死锁/雪崩。
                    // 故在子进程内强制把 cplex_threads 覆盖为 1（仅影响本子进程的 config 副本，
                    // 不影响父进程与全串行求解路径）。
                    Config childConfig = *config;
                    childConfig.cplex_threads = 1;
                    localBP.setConfig(childConfig);
                    // 子进程独立日志：每个个体写自己的文件，互不干扰、可完整回看 CG 明细
                    // fork 后子进程独占该 Logger，随子进程 _exit 一并销毁
                    mkdir(childLogDir.c_str(), 0755);
                    std::string childLogPath = childLogDir + "/run_gen"
                        + std::to_string(gen) + "_ind" + std::to_string(idx) + ".log";
                    Logger childLogger;
                    childLogger.init(childLogPath);
                    localBP.setLogger(&childLogger);
                    // 子进程内 CG 明细只写自己的日志文件，不写父终端（否则 N 进程刷屏乱序）
                    localBP.setConsoleProgress(false);
                    double result = localBP.solve(all_w[idx], "");

                    ChildResult payload{};
                    payload.fitness = result;
                    payload.bpTime = localBP.getBPTime();
                    payload.cgTime = localBP.getCGTime();
                    payload.psTime = localBP.getPricingTime();
                    payload.bbTime = localBP.getBBTime();
                    payload.cplexBuildTime = localBP.getCplexBuildTime();
                    payload.cplexSolveTime = localBP.getCplexSolveTime();
                    payload.cgIterations = localBP.getCGIterations();
                    payload.cgColumns = localBP.getCGTotalColumns();
                    payload.totalNodes = localBP.getTotalNodes();
                    payload.prunedNodes = localBP.getPrunedNodes();

                    ssize_t written = 0;
                    while (written < (ssize_t)sizeof(ChildResult)) {
                        ssize_t n = write(pipes[idx][1], (const char*)&payload + written, sizeof(ChildResult) - written);
                        if (n <= 0) break;
                        written += n;
                    }
                    close(pipes[idx][1]);
                    childLogger.close();  // _exit 不 flush 流缓冲，需手动刷盘保证子进程日志完整落盘
                    _exit(0);
                }
                close(pipes[idx][1]);
                childPids[idx] = pid;
                running++;
                genForkTime += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - forkStart).count();
                return true;
            };

            auto readChild = [&](int idx) {
                auto mergeStart = std::chrono::steady_clock::now();
                ChildResult payload{};
                ssize_t totalRead = 0;
                while (totalRead < (ssize_t)sizeof(ChildResult)) {
                    ssize_t n = read(pipes[idx][0], (char*)&payload + totalRead, sizeof(ChildResult) - totalRead);
                    if (n <= 0) break;
                    totalRead += n;
                }
                close(pipes[idx][0]);
                if (totalRead == (ssize_t)sizeof(ChildResult)) {
                    fitness[idx] = payload.fitness;
                    genBpTime += payload.bpTime;
                    genCgTime += payload.cgTime;
                    genPsTime += payload.psTime;
                    genBbTime += payload.bbTime;
                    genCplexBuildTime += payload.cplexBuildTime;
                    genCplexSolveTime += payload.cplexSolveTime;
                    genCgIterations += payload.cgIterations;
                    genCgColumns += payload.cgColumns;
                    genTotalNodes += payload.totalNodes;
                    genPrunedNodes += payload.prunedNodes;
                } else {
                    fitness[idx] = -DBL_MAX;
                }
                genMergeTime += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - mergeStart).count();
            };

            while (nextIdx < popSize && running < numThreads) {
                if (!launchChild(nextIdx)) {
                    std::cerr << "[GA] fork: 子进程启动失败" << std::endl;
                    fitness[nextIdx] = -DBL_MAX;
                    targetCompleted--;
                }
                nextIdx++;
            }

            int completed = 0;
            while (completed < targetCompleted) {
                int status;
                auto waitStart = std::chrono::steady_clock::now();
                pid_t endedPid = waitpid(-1, &status, 0);
                genWaitTime += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - waitStart).count();
                if (endedPid <= 0) break;
                for (int i = 0; i < popSize; i++) {
                    if (childPids[i] == endedPid) {
                        readChild(i);
                        childPids[i] = -1;
                        running--;
                        completed++;
                        while (nextIdx < popSize && running < numThreads) {
                            if (!launchChild(nextIdx)) {
                                std::cerr << "[GA] fork: 子进程启动失败" << std::endl;
                                fitness[nextIdx] = -DBL_MAX;
                                targetCompleted--;
                            }
                            nextIdx++;
                        }
                        break;
                    }
                }
            }

            parallelBpTime_ += genBpTime;
            parallelCgTime_ += genCgTime;
            parallelPsTime_ += genPsTime;
            parallelBbTime_ += genBbTime;
            parallelCplexBuildTime_ += genCplexBuildTime;
            parallelCplexSolveTime_ += genCplexSolveTime;
            parallelCgIterations_ += genCgIterations;
            parallelCgColumns_ += genCgColumns;
            parallelTotalNodes_ += genTotalNodes;
            parallelPrunedNodes_ += genPrunedNodes;

            // 本代 fork 段整体墙钟（含 CPU 调度等待，无法归入任何分项的差额即真实并行开销）
            double genWall = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - genWallStart).count();
            parallelForkTime_ += genForkTime;
            parallelWaitTime_ += genWaitTime;
            parallelMergeTime_ += genMergeTime;
            parallelWallTime_ += genWall;
            serialEquivBpTime_ += genBpTime;  // 各子进程 BP 之和 = 串行跑完这批的等效时间
            parallelGenerations_++;

            // 暴露给 run() 打印每代并行流水
            lastGenWall_ = genWall;
            lastGenSerialEquiv_ = genBpTime;
            lastGenForkTime_ = genForkTime;
            lastGenWaitTime_ = genWaitTime;
            lastGenMergeTime_ = genMergeTime;
            lastGenCgIterations_ = genCgIterations;
            lastGenCgColumns_ = genCgColumns;
            lastGenTotalNodes_ = genTotalNodes;
            lastGenParallel_ = true;
        } else {
            // 串行评估：与并行 fork 的 readChild 完全对称——逐个体取「本次求解增量」
            // 累加到 serial* 累加器，缓存命中（lastCallHitCache）不计入，
            // 使串行上报给 monitor 的 CG 迭代口径与并行一致。
            for (int i = 0; i < popSize; i++) {
                std::string label = "个体 #" + std::to_string(i);

                double result = bp.solve(all_w[i], label);

                fitness[i] = result;

                // 仅统计真实发生求解的个体（缓存命中直接返回，不产生 CG 迭代）
                if (!bp.lastCallHitCache()) {
                    serialCgIterations_ += bp.getLastCGIterations();
                    serialCgColumns_ += bp.getLastCGColumns();
                    serialTotalNodes_ += bp.getLastTotalNodes();
                    serialPrunedNodes_ += bp.getLastPrunedNodes();
                }
            }
            lastGenParallel_ = false;
        }

        auto evalEnd = std::chrono::steady_clock::now();
        accumulatedEvalTime_ += std::chrono::duration<double>(evalEnd - evalStart).count();

        return fitness;
    }

    Solution solveWithDetailedResult(const std::vector<double>& w) {
        // 最终重新评估：日志只写文件（run.log），不输出终端
        bool wasSilent = logger ? logger->isSilent() : false;
        if (logger) logger->setSilent(true);
        // 临时摘除 monitor：此处仅为拿最优解详情做「1 次」重评估，
        // 若保留 monitor，bp.solve() 内会用主 bp 对象的 accCGIterations_/accTotalNodes_
        // 覆盖 run() 中已上报的 parallel*/serial* 累计统计。并行模式下主 bp 只跑这 1 次，
        // 会把 CG/BB 误报成单次求解值（如 CG=28、BB节点=0），故重评估期间切断上报。
        bp.setMonitor(nullptr);
        double result = bp.solve(w, "最优解重新评估");
        bp.setMonitor(monitor);
        if (logger) logger->setSilent(wasSilent);
        Solution sol = bp.getBestSolution();
        sol.totalProfit = result;
        sol.totalInvestmentCost = 0.0;
        sol.numRtsServed = 0;
        for (const auto& dcSol : sol.dcSolutions) {
            for (int s : dcSol.S) {
                if (s == 1) sol.numRtsServed++;
            }
        }
        for (int j = 0; j < inst->numDC; j++) {
            // 使用公式层计算投资成本：½ * delta * w_j² * √D_j（论文标准版）
            double D_j = 0.0;
            for (const auto& dcSol : sol.dcSolutions) {
                if (dcSol.dcIndex == j) {
                    for (int i = 0; i < inst->numRetailer; i++) {
                        if (dcSol.S[i] == 1) D_j += inst->mu[i];
                    }
                    break;
                }
            }
            sol.totalInvestmentCost += InvestmentCostFormula::compute(
                *inst, j, w[j], D_j, config->use_sqrt_investment,
                config->investment_exponent);
        }
        sol.w = w;
        return sol;
    }

    // ===== 遗传算子 =====

    std::vector<std::vector<double>> selectParents(
        const std::vector<std::vector<double>>& pop,
        const std::vector<double>& realFitness) {

        int popSize = pop.size();
        std::vector<double> fitness(popSize);
        double minFitness = *std::min_element(realFitness.begin(), realFitness.end());
        for (int i = 0; i < popSize; i++) {
            fitness[i] = std::max(0.0, realFitness[i] - minFitness);
        }

        double totalFitness = std::accumulate(fitness.begin(), fitness.end(), 0.0);
        if (totalFitness < 1e-15) {
            std::fill(fitness.begin(), fitness.end(), 1.0);
            totalFitness = popSize;
        }

        int geneLength = pop[0].size();
        std::vector<std::vector<double>> parents(popSize, std::vector<double>(geneLength));

        for (int i = 0; i < popSize; i++) {
            double spin = rng.uniform(0.0, totalFitness);
            double partialSum = 0.0;
            for (int j = 0; j < popSize; j++) {
                partialSum += fitness[j];
                if (partialSum >= spin) {
                    parents[i] = pop[j];
                    break;
                }
            }
        }

        return parents;
    }

    std::vector<std::vector<double>> crossover(
        const std::vector<std::vector<double>>& parents) {

        int popSize = parents.size();
        int geneLength = parents[0].size();
        std::vector<std::vector<double>> offspring(popSize, std::vector<double>(geneLength));

        for (int i = 0; i < popSize - 1; i += 2) {
            if (rng.uniform() < config->crossover_rate) {
                // 两点交叉
                int point1 = rng.uniformInt(0, geneLength - 2);
                int point2 = rng.uniformInt(point1 + 1, geneLength - 1);
                for (int j = 0; j < geneLength; j++) {
                    if (j >= point1 && j < point2) {
                        offspring[i][j] = parents[i + 1][j];
                        offspring[i + 1][j] = parents[i][j];
                    } else {
                        offspring[i][j] = parents[i][j];
                        offspring[i + 1][j] = parents[i + 1][j];
                    }
                }
            } else {
                offspring[i] = parents[i];
                if (i + 1 < popSize) {
                    offspring[i + 1] = parents[i + 1];
                }
            }
        }

        if (popSize % 2 == 1) {
            offspring[popSize - 1] = parents[popSize - 1];
        }

        return offspring;
    }

    void mutate(std::vector<std::vector<double>>& offspring) {
        if (offspring.empty() || offspring[0].empty()) return;
        double mutationRate =
            config->mutation_rate > 0.0
                ? config->mutation_rate
                : 1.0 / static_cast<double>(offspring[0].size());
        for (size_t i = 0; i < offspring.size(); i++) {
            for (size_t j = 0; j < offspring[i].size(); j++) {
                if (rng.uniform() < mutationRate) {
                    offspring[i][j] = 1.0 - offspring[i][j];
                }
            }
        }
    }

    std::vector<std::vector<double>> replacePopulation(
        const std::vector<std::vector<double>>& pop,
        const std::vector<double>& fitness,
        const std::vector<std::vector<double>>& offspring) {

        int popSize = pop.size();
        int geneLength = pop[0].size();
        std::vector<std::vector<double>> newPop(popSize, std::vector<double>(geneLength));

        int bestIdx = 0;
        for (int i = 1; i < popSize; i++) {
            if (fitness[i] > fitness[bestIdx]) {
                bestIdx = i;
            }
        }

        if (config->elitism) {
            newPop[0] = pop[bestIdx];
            int targetIdx = 1;
            for (int i = 0; i < popSize && targetIdx < popSize; i++) {
                if (i != bestIdx) {
                    newPop[targetIdx] = offspring[i];
                    targetIdx++;
                }
            }
        } else {
            newPop = offspring;
        }

        return newPop;
    }

    double getElapsedSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - startTime).count();
    }
};

#endif // GENETIC_ALGORITHM_H
