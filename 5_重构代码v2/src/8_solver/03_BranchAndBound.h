#ifndef BRANCH_AND_BOUND_H
#define BRANCH_AND_BOUND_H

#include "../10_common/Instance.h"
#include "../10_common/Types.h"
#include "../10_common/Config.h"
#include "../4_logger/Logger.h"
#include "02_ColumnGeneration.h"
#include <vector>
#include <queue>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>

/**
 * 分支定界求解器（Branch and Bound）
 *
 * 在列生成得到LP松弛解后，如果解不是整数，则进入分支定界。
 *
 * 修复了原代码的核心问题：
 * 1. 正确维护全局上下界：bestLowerBound（全局最大下界，取所有整数解的最大值）
 *    bestUpperBound（全局最小上界，取所有活跃节点LP解的最小值）
 * 2. 剪枝条件：如果当前节点的LP解 ≤ bestLowerBound，则剪枝
 * 3. 分支策略：深度优先（DFS）优先搜索，更快找到整数解更新下界
 * 4. 节点选择：选择上界最大的节点（最有希望的）优先分支
 *
 * 分支规则：
 *   对分数变量 z_j^k 分支：
 *   左分支：z_j^k = 0
 *   右分支：z_j^k = 1
 */
class BranchAndBound {
private:
    const Instance* inst;
    const Config* config;
    ColumnGeneration columnGen;
    ObjectiveFormula objFormula;
    Logger* logger;

    // 统计信息
    int totalNodes;
    int prunedNodes;
    int integerSolutions;
    double bestLowerBound;   // 全局最大下界（找到的最好整数解）
    double bestUpperBound;   // 全局最小上界（根节点LP解）
    Solution bestSolution;   // 最优整数解
    double bbTime;           // 分支定界耗时

    // 列生成结果缓存（避免重新生成列）
    struct BnBNode {
        BranchState state;
        CGResult cgResult;

        BnBNode(const BranchState& s, const CGResult& cg)
            : state(s), cgResult(cg) {}
    };

public:
    BranchAndBound() : inst(nullptr), config(nullptr), logger(nullptr),
                       totalNodes(0), prunedNodes(0), integerSolutions(0),
                       bestLowerBound(-DBL_MAX), bestUpperBound(DBL_MAX) {}

    void setLogger(Logger* log) {
        logger = log;
        columnGen.setLogger(log);
    }

    void setInstance(const Instance& instance) {
        inst = &instance;
        columnGen.setInstance(instance);
    }

    void setConfig(const Config& cfg) {
        config = &cfg;
        rc_eps = cfg.rc_eps;
        columnGen.setConfig(cfg);
        columnGen.setRCEps(cfg.rc_eps);
    }

    /** 获取统计信息 */
    int getTotalNodes() const { return totalNodes; }
    int getPrunedNodes() const { return prunedNodes; }
    int getIntegerSolutions() const { return integerSolutions; }
    double getBestLowerBound() const { return bestLowerBound; }
    double getBestUpperBound() const { return bestUpperBound; }
    const Solution& getBestSolution() const { return bestSolution; }
    double getBBTime() const { return bbTime; }
    /** 获取内部列生成的时间统计 */
    double getCGTime() const { return columnGen.getCGTime(); }
    double getPricingTime() const { return columnGen.getPricingTime(); }
    double getCplexBuildTime() const { return columnGen.getCplexBuildTime(); }
    double getCplexSolveTime() const { return columnGen.getCplexSolveTime(); }
    int getCGIterations() const { return columnGen.getIterations(); }
    int getCGTotalColumns() const { return columnGen.getTotalColumns(); }

    /**
     * 求解分支定界
     *
     * 注意：w 已由 BranchAndPrice 通过 Instance::setEmissionReductionRates 设置
     * @return 最优解的目标函数值（不含碳配额收入）
     */
    double solve() {
        auto start = std::chrono::steady_clock::now();

        // 重置状态
        totalNodes = 0;
        prunedNodes = 0;
        integerSolutions = 0;
        bestLowerBound = -DBL_MAX;
        bestUpperBound = DBL_MAX;

        // 重置列生成和定价的统计累加器（确保多次BP调用间统计正确）
        columnGen.resetStats();

        // ===== 1. 求解根节点（输出进度到终端） =====
        BranchState rootState;
        columnGen.setConsoleProgress(true);
        CGResult rootResult = columnGen.solve(rootState);

        if (!rootResult.feasible) {
            return -DBL_MAX;  // 不可行
        }

        // 根节点LP解作为初始上界
        bestUpperBound = rootResult.lpObjective;

        // ===== 2. 检查根节点是否为整数解 =====
        if (rootResult.isInteger) {
            bestLowerBound = rootResult.lpObjective;
            saveIntegerSolution(rootResult);
            auto end = std::chrono::steady_clock::now();
            if (logger) logger->progress(3, "根节点即为整数解，obj=" + std::to_string(bestLowerBound));
            return bestLowerBound;
        }

        // ===== 3. 分支定界搜索 =====
        if (logger) {
            logger->progress(3, "分支定界开始，根节点LP obj=" + std::to_string(rootResult.lpObjective)
                + "，最大节点数=" + std::to_string(config->max_branch_nodes));
        }

        // 使用栈（DFS）实现深度优先搜索
        std::vector<BnBNode> nodeStack;

        // 将根节点入栈
        nodeStack.emplace_back(rootState, rootResult);

        while (!nodeStack.empty()) {
            // 取出栈顶节点（DFS）
            BnBNode currentNode = nodeStack.back();
            nodeStack.pop_back();

            // 剪枝检查：如果当前节点的LP解 ≤ bestLowerBound，则剪枝
            if (currentNode.cgResult.lpObjective <= bestLowerBound + 1e-6) {
                prunedNodes++;
                continue;
            }

            // 如果节点数超过限制，停止分支
            if (totalNodes >= config->max_branch_nodes) {
                prunedNodes += nodeStack.size();
                if (logger) logger->progress(3, "达到最大节点数限制("
                    + std::to_string(config->max_branch_nodes) + ")，停止分支");
                break;
            }

            // ===== 4. 选择分支变量 =====
            int id_j = -1;
            int id_S = -1;
            double max_frac = 0.0;

            for (int j = 0; j < (int)currentNode.cgResult.final_z_j.size(); j++) {
                for (int k = 0; k < (int)currentNode.cgResult.final_z_j[j].size(); k++) {
                    double val = currentNode.cgResult.final_z_j[j][k];
                    double frac = std::abs(val - std::round(val));
                    if (frac > rc_eps && frac > max_frac) {
                        max_frac = frac;
                        id_j = j;
                        id_S = k;
                    }
                }
            }

            if (id_j == -1) {
                // 所有变量都是整数（理论上不应发生）
                if (currentNode.cgResult.lpObjective > bestLowerBound) {
                    bestLowerBound = currentNode.cgResult.lpObjective;
                    saveIntegerSolution(currentNode.cgResult);
                    integerSolutions++;
                }
                continue;
            }

            totalNodes++;

            // ===== 5. 生成两个子节点：z_j^k = 0 和 z_j^k = 1 =====

            // 分支节点不输出CG进度到终端（只写文件）
            columnGen.setConsoleProgress(false);

            // 5a. 左分支：z = 0
            {
                BranchState childState = currentNode.state.branch(id_j, id_S, 0);
                CGResult childResult = columnGen.solve(childState);

                if (childResult.feasible) {
                    if (childResult.isInteger) {
                        // 整数解：更新下界
                        if (childResult.lpObjective > bestLowerBound) {
                            bestLowerBound = childResult.lpObjective;
                            saveIntegerSolution(childResult);
                            integerSolutions++;
                        }
                    } else if (childResult.lpObjective > bestLowerBound + 1e-6) {
                        // 非整数解且上界 > 下界：继续分支
                        nodeStack.emplace_back(childState, childResult);
                    } else {
                        prunedNodes++;
                    }
                } else {
                    prunedNodes++;
                }
            }

            // 5b. 右分支：z = 1
            {
                BranchState childState = currentNode.state.branch(id_j, id_S, 1);
                CGResult childResult = columnGen.solve(childState);

                if (childResult.feasible) {
                    if (childResult.isInteger) {
                        if (childResult.lpObjective > bestLowerBound) {
                            bestLowerBound = childResult.lpObjective;
                            saveIntegerSolution(childResult);
                            integerSolutions++;
                        }
                    } else if (childResult.lpObjective > bestLowerBound + 1e-6) {
                        nodeStack.emplace_back(childState, childResult);
                    } else {
                        prunedNodes++;
                    }
                } else {
                    prunedNodes++;
                }
            }
        }

        auto end = std::chrono::steady_clock::now();

        // 如果没找到整数解，返回根节点LP解作为近似
        if (bestLowerBound <= -DBL_MAX / 2) {
            bestLowerBound = rootResult.lpObjective;
        }

        if (logger) {
            logger->progress(3, "分支定界结束：探索 " + std::to_string(totalNodes)
                + " 节点，剪枝 " + std::to_string(prunedNodes)
                + "，整数解 " + std::to_string(integerSolutions)
                + "，最优 obj=" + std::to_string(bestLowerBound));
        }

        return bestLowerBound;
    }

private:
    double rc_eps = 1.0e-6;

    /**
     * 保存整数解到 Solution 结构
     */
    void saveIntegerSolution(const CGResult& cgResult) {
        bestSolution = Solution();
        bestSolution.totalProfit = cgResult.lpObjective;

        // 统计信息
        int numRetailer = inst->numRetailer;

        for (int j = 0; j < (int)cgResult.final_z_j.size(); j++) {
            for (int k = 0; k < (int)cgResult.final_z_j[j].size(); k++) {
                if (cgResult.final_z_j[j][k] > 0.5) {
                    // 该列被选中
                    DCSolution dcSol;
                    dcSol.dcIndex = j;
                    dcSol.w = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
                    dcSol.p = cgResult.p_j[j][k];
                    dcSol.S = cgResult.j_S[j][k];
                    dcSol.profit = objFormula.columnProfit(*inst, j, dcSol.S, dcSol.p, inst->w[j]);
                    bestSolution.dcSolutions.push_back(dcSol);
                    bestSolution.numDCsOpen++;
                }
            }
        }

        bestSolution.w = inst->w;
    }
};

#endif // BRANCH_AND_BOUND_H
