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
#include <future>
#include <mutex>
// fork 并行评估所需头文件
#include <unistd.h>
#include <sys/wait.h>
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

        bestFitness = -DBL_MAX;
        bestW.resize(inst->numDC, 0.0);

        // ===== 1. 初始化种群 =====
        population = initializePopulation(popSize, geneLength);

        std::cout << "[GA] 遗传算法开始，种群大小=" << popSize
                  << "，最大代数=" << maxGen
                  << "，编码位数=" << chromLen << std::endl;

        // ===== 2. 主迭代 =====
        int noImproveCount = 0;
        double prevBest = -DBL_MAX;

        for (int gen = 0; gen < maxGen; gen++) {
            auto genStart = std::chrono::steady_clock::now();

            // 2a. 计算适应度
            std::vector<double> fitness = calculateFitness(population);

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

            std::cout << "代数 " << gen << ": Best=" << std::fixed << std::setprecision(2)
                      << bestFitness << ", Avg=" << genAvg
                      << ", Worst=" << genWorst
                      << ", 耗时=" << genTime << "s";

            if (config->early_stop && noImproveCount > 0) {
                std::cout << " (连续" << noImproveCount << "代未提升";
                if (noImproveCount >= config->convergence_generations) {
                    std::cout << " → 提前停止";
                }
                std::cout << ")";
            }
            std::cout << std::endl;

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
                // 并行模式：BP 时间来自各线程，无法精确细分，用适应度评估总时间近似
                monitor->reportBPTiming(accumulatedEvalTime_);
                // CG/PS/BB 在并行模式下来自独立线程实例，不做细分统计
            } else {
                // 串行模式：所有 BP 调用都通过主 bp 对象，各时间累加正确
                monitor->reportBPTiming(bp.getBPTime());
                monitor->reportCGTiming(bp.getCGTime(),
                                         bp.getCplexBuildTime(),
                                         bp.getCplexSolveTime());
                monitor->reportPricingTiming(bp.getPricingTime());
                monitor->reportBBTiming(bp.getBBTime());
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
    std::vector<double> calculateFitness(const std::vector<std::vector<double>>& pop) {
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

        // 计时：BP 求解（适应度评估）
        auto evalStart = std::chrono::steady_clock::now();

        if (parallel && popSize > 1 && numThreads > 1) {
            // ===== 多进程（fork）并行评估 =====
            // 每个子进程有独立的 CPLEX 全局状态和内存空间，完全避开 CPLEX 多线程 bug
            //
            // 设计：
            //   1. 每个个体创建一个 pipe（父子进程通信）
            //   2. fork() 产生子进程，子进程执行 BP 求解
            //   3. 结果通过 pipe 传回父进程（一个 double）
            //   4. 父进程 waitpid 回收子进程
            //   5. 同时最多 numThreads 个子进程运行

            std::vector<int> childPids(popSize, -1);
            // 每个 pipe 有两个 fd: [0]=读端(父), [1]=写端(子)
            std::vector<std::array<int, 2>> pipes(popSize);

            // 子进程计数信号量（简单实现：同时最多 N 个）
            int running = 0;
            int nextIdx = 0;  // 下一个要启动的子进程索引

            // 累计时间统计（子进程无法传回详细时间，只传 fitness）
            // 时间统计由父进程在串行段累积
            double paraBpTime = 0.0;
            double paraCgTime = 0.0;
            double paraPsTime = 0.0;
            double paraBbTime = 0.0;

            // 启动第一批子进程
            while (nextIdx < popSize && running < numThreads) {
                if (pipe(pipes[nextIdx].data()) == -1) {
                    std::cerr << "[GA] fork: pipe 创建失败" << std::endl;
                    fitness[nextIdx] = -DBL_MAX;
                    nextIdx++;
                    continue;
                }

                pid_t pid = fork();
                if (pid == -1) {
                    std::cerr << "[GA] fork: fork 失败" << std::endl;
                    close(pipes[nextIdx][0]);
                    close(pipes[nextIdx][1]);
                    fitness[nextIdx] = -DBL_MAX;
                    nextIdx++;
                    continue;
                }

                if (pid == 0) {
                    // ===== 子进程 =====
                    // 关闭读端（子进程只写）
                    close(pipes[nextIdx][0]);

                    // 子进程需要独立的数据副本（fork 后 COW，但 CPLEX 会写内存）
                    Instance localInst = inst->deepCopy();
                    BranchAndPrice localBP;
                    localBP.setInstance(localInst);
                    localBP.setConfig(*config);
                    // 子进程不输出日志到终端
                    localBP.setLogger(nullptr);

                    double result = localBP.solve(all_w[nextIdx], "");

                    // 结果写入 pipe
                    ssize_t written = 0;
                    while (written < (ssize_t)sizeof(double)) {
                        ssize_t n = write(pipes[nextIdx][1],
                                          (const char*)&result + written,
                                          sizeof(double) - written);
                        if (n <= 0) break;
                        written += n;
                    }

                    // 关闭写端，退出子进程
                    close(pipes[nextIdx][1]);
                    _exit(0);
                }

                // ===== 父进程 =====
                // 关闭写端（父进程只读）
                close(pipes[nextIdx][1]);
                childPids[nextIdx] = pid;
                running++;
                nextIdx++;
            }

            // 收集所有结果（边等边启动新子进程）
            int completed = 0;
            while (completed < popSize) {
                // 等待任意子进程结束
                int status;
                pid_t endedPid = waitpid(-1, &status, 0);
                if (endedPid <= 0) {
                    // 没有子进程了
                    break;
                }

                // 找到这个 pid 对应的索引
                for (int i = 0; i < popSize; i++) {
                    if (childPids[i] == endedPid) {
                        // 从 pipe 读取结果
                        double childResult = -DBL_MAX;
                        ssize_t totalRead = 0;
                        while (totalRead < (ssize_t)sizeof(double)) {
                            ssize_t n = read(pipes[i][0],
                                            (char*)&childResult + totalRead,
                                            sizeof(double) - totalRead);
                            if (n <= 0) break;
                            totalRead += n;
                        }
                        close(pipes[i][0]);
                        fitness[i] = childResult;
                        childPids[i] = -1;  // 标记已回收
                        completed++;
                        running--;

                        // 启动下一个待处理的个体
                        if (nextIdx < popSize) {
                            if (pipe(pipes[nextIdx].data()) == -1) {
                                std::cerr << "[GA] fork: pipe 创建失败" << std::endl;
                                fitness[nextIdx] = -DBL_MAX;
                                nextIdx++;
                                break;
                            }

                            pid_t newPid = fork();
                            if (newPid == -1) {
                                std::cerr << "[GA] fork: fork 失败" << std::endl;
                                close(pipes[nextIdx][0]);
                                close(pipes[nextIdx][1]);
                                fitness[nextIdx] = -DBL_MAX;
                                nextIdx++;
                                break;
                            }

                            if (newPid == 0) {
                                // 子进程
                                close(pipes[nextIdx][0]);
                                Instance localInst = inst->deepCopy();
                                BranchAndPrice localBP;
                                localBP.setInstance(localInst);
                                localBP.setConfig(*config);
                                localBP.setLogger(nullptr);
                                double result = localBP.solve(all_w[nextIdx], "");
                                ssize_t written = 0;
                                while (written < (ssize_t)sizeof(double)) {
                                    ssize_t n = write(pipes[nextIdx][1],
                                                    (const char*)&result + written,
                                                    sizeof(double) - written);
                                    if (n <= 0) break;
                                    written += n;
                                }
                                close(pipes[nextIdx][1]);
                                _exit(0);
                            }

                            // 父进程
                            close(pipes[nextIdx][1]);
                            childPids[nextIdx] = newPid;
                            running++;
                            nextIdx++;
                        }
                        break;
                    }
                }
            }

            // 清理：如果还有未回收的子进程
            for (int i = 0; i < popSize; i++) {
                if (childPids[i] > 0) {
                    waitpid(childPids[i], nullptr, 0);
                    close(pipes[i][0]);
                }
            }

            // 时间统计在 fork 模式下不精确（子进程时间无法累加到父进程）
            // 用粗略估计：用串行段的时间代替
            if (monitor) {
                // 大致估计：每个个体平均分配总评估时间
                double evalTime = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - evalStart).count();
                monitor->reportBPTiming(evalTime * 0.9); // BP 占绝大部分
            }
        } else {
            // 串行评估
            for (int i = 0; i < popSize; i++) {
                std::string label = "个体 #" + std::to_string(i);

                double result = bp.solve(all_w[i], label);

                fitness[i] = result;
            }
        }

        auto evalEnd = std::chrono::steady_clock::now();
        accumulatedEvalTime_ += std::chrono::duration<double>(evalEnd - evalStart).count();

        return fitness;
    }

    Solution solveWithDetailedResult(const std::vector<double>& w) {
        // 最终重新评估：日志只写文件（run.log），不输出终端
        bool wasSilent = logger ? logger->isSilent() : false;
        if (logger) logger->setSilent(true);
        double result = bp.solve(w, "最优解重新评估");
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
                *inst, j, w[j], D_j, config->use_sqrt_investment);
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
        for (size_t i = 0; i < offspring.size(); i++) {
            for (size_t j = 0; j < offspring[i].size(); j++) {
                if (rng.uniform() < config->mutation_rate) {
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
