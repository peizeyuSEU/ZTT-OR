#ifndef GENETIC_ALGORITHM_H
#define GENETIC_ALGORITHM_H

#include "../0_base/01_types/Instance.h"
#include "../0_base/01_types/Types.h"
#include "../0_base/01_types/Solution.h"
#include "../2_config/Config.h"
#include "../0_base/03_utils/Random.h"
#include "../0_base/03_utils/Metrics.h"
#include "../0_base/03_utils/Logger.h"
#include "../0_base/03_utils/ThreadPool.h"
#include "../3_formula/08_Objective.h"
#include "../3_formula/05_InvestmentCost.h"
#include "04_BranchAndPrice.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <functional>
#include <iomanip>
#include <future>

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
    Metrics metrics;

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

public:
    GeneticAlgorithm(const Config& cfg, Instance& instance)
        : config(&cfg), inst(&instance), rng(cfg.random_seed), logger(nullptr),
          bestFitness(-DBL_MAX) {

        bp.setInstance(instance);
        bp.setConfig(cfg);
        startTime = std::chrono::steady_clock::now();
    }

    void setLogger(Logger* log) {
        logger = log;
        bp.setLogger(log);
    }

    Metrics& getMetrics() { return metrics; }
    const std::vector<ConvergencePoint>& getConvergence() const { return convergence; }
    double getBestFitness() const { return bestFitness; }
    const std::vector<double>& getBestW() const { return bestW; }
    const Solution& getBestSolution() const { return bestSolution; }

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

        if (logger) {
            logger->log(0, "遗传算法开始，种群大小=" + std::to_string(popSize)
                        + "，最大代数=" + std::to_string(maxGen)
                        + "，编码位数=" + std::to_string(chromLen));
        }

        // ===== 2. 主迭代 =====
        int noImproveCount = 0;
        double prevBest = -DBL_MAX;

        for (int gen = 0; gen < maxGen; gen++) {
            auto genStart = std::chrono::steady_clock::now();

            if (logger) {
                logger->log(0, "第 " + std::to_string(gen) + " 代开始");
            }

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

            // 2f. 输出当前代信息
            auto genEnd = std::chrono::steady_clock::now();
            double genTime = std::chrono::duration<double>(genEnd - genStart).count();

            if (logger) {
                logger->log(0, "第 " + std::to_string(gen) + " 代最优适应度 = "
                            + std::to_string(bestFitness));
            }

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
                if (logger) logger->log(0, "提前停止：连续 " + std::to_string(noImproveCount)
                            + " 代未提升");
                break;
            }

            // 2g. 遗传操作（最后一代不进行）
            if (gen < maxGen - 1) {
                if (logger) logger->log(0, "执行选择、交叉、变异");
                std::vector<double> realFitness = fitness;
                std::vector<std::vector<double>> parents = selectParents(population, realFitness);
                std::vector<std::vector<double>> offspring = crossover(parents);
                mutate(offspring);
                population = replacePopulation(population, fitness, offspring);
            }
        }

        // ===== 3. 输出最终结果 =====
        auto end = std::chrono::steady_clock::now();
        double totalTime = std::chrono::duration<double>(end - startTime).count();

        if (logger) {
            logger->log(0, "遗传算法收敛，最优w=[" + wToString(bestW)
                        + "]，最优利润=" + std::to_string(bestFitness));
        }

        std::cout << "\n========== 遗传算法完成 ==========" << std::endl;
        std::cout << "总耗时: " << totalTime << "s" << std::endl;
        std::cout << "最优适应度: " << std::fixed << std::setprecision(2) << bestFitness << std::endl;
        std::cout << "最优w: ";
        for (int j = 0; j < inst->numDC; j++) {
            std::cout << "w" << j << "=" << bestW[j] << " ";
        }
        std::cout << std::endl;

        // ===== 4. 用最优w重新求解，输出详细结果 =====
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

    double binaryToDecimal(const std::vector<double>& binary,
                            double minVal, double maxVal, int chromLen) {
        double decimal = 0.0;
        for (int i = 0; i < chromLen; i++) {
            decimal += binary[i] * std::pow(2.0, chromLen - 1 - i);
        }
        return minVal + decimal / (std::pow(2.0, chromLen) - 1.0) * (maxVal - minVal);
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

        // 先把所有 w 向量算好
        std::vector<std::vector<double>> all_w(popSize);
        for (int i = 0; i < popSize; i++) {
            all_w[i].resize(inst->numDC);
            for (int j = 0; j < inst->numDC; j++) {
                std::vector<double> v(pop[i].begin() + j * config->chromosome_length,
                                      pop[i].begin() + (j + 1) * config->chromosome_length);
                all_w[i][j] = binaryToDecimal(v, config->min_w, config->max_w, config->chromosome_length);
            }
        }

        if (parallel && popSize > 1 && numThreads > 1) {
            // 并行评估
            ThreadPool pool(numThreads);
            std::vector<std::future<double>> futures(popSize);

            for (int i = 0; i < popSize; i++) {
                futures[i] = pool.enqueue([this, i, &all_w]() -> double {
                    // 每个线程使用独立的 Instance 副本 + 独立的 BranchAndPrice
                    Instance localInst = inst->clone();
                    BranchAndPrice localBP;
                    localBP.setInstance(localInst);
                    localBP.setConfig(*config);
                    double result = localBP.solve(all_w[i], "");
                    return result;
                });
            }

            for (int i = 0; i < popSize; i++) {
                fitness[i] = futures[i].get();
            }
        } else {
            // 串行评估
            for (int i = 0; i < popSize; i++) {
                std::string label = "个体 #" + std::to_string(i);

                double result = bp.solve(all_w[i], label);

                fitness[i] = result;
            }
        }

        return fitness;
    }

    Solution solveWithDetailedResult(const std::vector<double>& w) {
        double result = bp.solve(w, "最优解重新评估");
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
