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
    SolveStatus solveStatus;
    double finalRelativeGap;

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
                       bestLowerBound(-DBL_MAX), bestUpperBound(DBL_MAX),
                       bbTime(0.0), solveStatus(SolveStatus::NOT_STARTED),
                       finalRelativeGap(DBL_MAX) {}

    void setLogger(Logger* log) {
        logger = log;
        columnGen.setLogger(log);
    }

    /** 透传：控制 CG 迭代进度是否写终端（子进程并行时关掉，只写各自日志文件） */
    void setConsoleProgress(bool on) { columnGen.setConsoleProgress(on); }

    void setInstance(const Instance& instance) {
        inst = &instance;
        columnGen.setInstance(instance);
    }

    void setConfig(const Config& cfg) {
        config = &cfg;
        objFormula.setConfig(cfg);
        rc_eps = cfg.rc_eps;
        columnGen.setConfig(cfg);
        columnGen.setRCEps(cfg.rc_eps);

        // ===== 环节配置横幅：BB 层（第2层，DFS 分支定界恢复整数解，只打印一次） =====
        static bool bbBannerPrinted = false;
        if (!bbBannerPrinted) {
            std::cout << "\n    ========== [环节3/5] 分支定界 BB（DFS 深度优先，恢复整数解） ==========" << std::endl;
            std::cout << "      设计: 根节点列生成求 LP → 按 x_ij(DC-零售商对)分数值分支 → 左 x_ij=0 / 右 x_ij=1" << std::endl;
            std::cout << "      策略: 深度优先(DFS 栈) | 选最大分数化变量分支 | LP≤下界则剪枝" << std::endl;
            std::cout << "      认证: 所有节点严格列生成 | 目标gap=" << cfg.bp_relative_gap
                      << " | 最大节点数=" << cfg.max_branch_nodes
                      << " | 时间上限=" << cfg.bp_time_limit_sec << "s" << std::endl;
            std::cout << "      资源限制仅改变求解状态，不允许LP值冒充整数可行解" << std::endl;
            bbBannerPrinted = true;
        }
    }

    /** 获取统计信息 */
    int getTotalNodes() const { return totalNodes; }
    int getPrunedNodes() const { return prunedNodes; }
    int getIntegerSolutions() const { return integerSolutions; }
    double getBestLowerBound() const { return bestLowerBound; }
    double getBestUpperBound() const { return bestUpperBound; }
    const Solution& getBestSolution() const { return bestSolution; }
    double getBBTime() const { return bbTime; }
    SolveStatus getSolveStatus() const { return solveStatus; }
    double getRelativeGap() const { return finalRelativeGap; }
    bool hasIntegerSolution() const { return bestLowerBound > -DBL_MAX / 2; }
    /** 获取内部列生成的时间统计 */
    double getCGTime() const { return columnGen.getCGTime(); }
    double getPricingTime() const { return columnGen.getPricingTime(); }
    double getCplexBuildTime() const { return columnGen.getCplexBuildTime(); }
    double getCplexSolveTime() const { return columnGen.getCplexSolveTime(); }
    int getCGIterations() const { return columnGen.getIterations(); }
    int getCGTotalColumns() const { return columnGen.getTotalColumns(); }

    // ===== 细粒度诊断 getter 转发（临时） =====
    double getDbgDualTime() const { return columnGen.getDbgDualTime(); }
    double getDbgIntCheckTime() const { return columnGen.getDbgIntCheckTime(); }
    double getDbgFillTime() const { return columnGen.getDbgFillTime(); }
    double getDbgAddColTime() const { return columnGen.getDbgAddColTime(); }
    long long getDbgIntCheckCalls() const { return columnGen.getDbgIntCheckCalls(); }
    long long getDbgFillCalls() const { return columnGen.getDbgFillCalls(); }

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
        bestSolution = Solution();
        solveStatus = SolveStatus::NOT_STARTED;
        finalRelativeGap = DBL_MAX;

        // 重置列生成和定价的统计累加器（确保多次BP调用间统计正确）
        columnGen.resetStats();

        // ===== 1. 求解根节点（输出进度到终端） =====
        BranchState rootState;
        columnGen.setConsoleProgress(true);
        // 根节点强制走严格收敛出口（forceStrict=true 禁用 tailing-off 早停），
        // 使 rootResult.lpObjective 成为严格 LP 上界，避免被早停低估。
        CGResult rootResult = columnGen.solve(rootState, nullptr, true);

        if (!rootResult.feasible) {
            solveStatus = SolveStatus::INFEASIBLE;
            bestSolution.solveStatus = solveStatus;
            return -DBL_MAX;  // 不可行
        }
        if (!rootResult.certifiedOptimal) {
            solveStatus = SolveStatus::CG_ITERATION_LIMIT;
            bestSolution.solveStatus = solveStatus;
            return -DBL_MAX;
        }

        // 根节点LP解作为初始上界
        bestUpperBound = rootResult.lpObjective;

        // ===== 2. 检查根节点是否为整数解（方案 B：统一 x_ij 口径，不依赖 z 口径）=====
        {
            int rj = -1;
            int ri = -1;
            BranchState rootCheckState;
            bool rootFractional =
                selectBranchingVariable(rootResult, rootCheckState, rj, ri);
            if (!rootFractional) {
                bestLowerBound = rootResult.lpObjective;
                saveIntegerSolution(rootResult);
                bestUpperBound = bestLowerBound;
                finalRelativeGap = 0.0;
                solveStatus = SolveStatus::OPTIMAL;
                bestSolution.solveStatus = solveStatus;
                bestSolution.bestBound = bestUpperBound;
                bestSolution.relativeGap = 0.0;
                bestSolution.hasIntegerSolution = true;
                auto end = std::chrono::steady_clock::now();
                // 本次 BB 纯分支时间 = 总耗时 - 列生成耗时（此处根节点即整数解，无额外分支）
                double totalSec = std::chrono::duration<double>(end - start).count();
                bbTime = std::max(0.0, totalSec - columnGen.getCGTime());
                if (logger) logger->progress(3, "根节点即为整数解，obj=" + std::to_string(bestLowerBound));
                return bestLowerBound;
            }
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

        // ===== 方案B：根节点整数启发式，抬高 bestLowerBound 使剪枝尽早生效 =====
        if (config && config->root_heuristic) {
            double heurLB = roundingHeuristic(rootResult);
            if (heurLB > bestLowerBound) {
                bestLowerBound = heurLB;
                if (logger) logger->progress(3, "根节点启发式下界 LB="
                    + std::to_string(bestLowerBound)
                    + "（上界=" + std::to_string(bestUpperBound) + "）");
            }
        }
        if (config && config->root_rmp_mip_heuristic) {
            double mipLB = restrictedMasterMIPHeuristic(rootResult);
            if (mipLB > bestLowerBound) {
                bestLowerBound = mipLB;
                if (logger) logger->progress(3, "根节点0-1 RMP启发式下界 LB="
                    + std::to_string(bestLowerBound)
                    + "（上界=" + std::to_string(bestUpperBound) + "）");
            }
        }

        // 终止条件参数（防止无效分支导致长时间空转）
        const double kTimeLimitSec = config ? config->bp_time_limit_sec : 600.0;
        const double kGapTol = config ? std::max(0.0, config->bp_relative_gap) : 0.0;
        bool abortedByUncertifiedCG = false;
        long long hybridEligibleSelections = 0;
        long long nodeSelections = 0;
        long long lastIncumbentImprovementSelection = 0;
        long long lastAdaptiveRestartSelection = -1000000000LL;

        while (!nodeStack.empty()) {
            // 时间上限终止
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start).count();
            if (kTimeLimitSec > 0.0 && elapsed >= kTimeLimitSec) {
                prunedNodes += (int)nodeStack.size();
                solveStatus = SolveStatus::TIME_LIMIT;
                if (logger) logger->progress(3, "达到时间上限("
                    + std::to_string((int)kTimeLimitSec) + "s)，停止分支");
                break;
            }

            // ===== 上界动态下压 =====
            // 分支定界任一时刻的真实上界 = 所有活节点（栈中未处理）LP 松弛值的最大值。
            // 子节点是父节点加约束后的更紧 LP，最大化问题下 LP 单调不增，
            // 故活节点最大 LP ≤ 根 LP，可随搜索推进不断下压天花板。
            // 栈空时无活节点，上界维持为已知最优下界（gap→0，自然收敛）。
            {
                double activeMaxLp = -DBL_MAX;
                for (const BnBNode& node : nodeStack) {
                    if (node.cgResult.lpObjective > activeMaxLp) {
                        activeMaxLp = node.cgResult.lpObjective;
                    }
                }
                if (activeMaxLp > -DBL_MAX / 2 && activeMaxLp < bestUpperBound) {
                    bestUpperBound = activeMaxLp;
                }
            }

            // gap 终止：上界(活节点最大LP，单调不增)与已知最优下界之间的相对间隙足够小。
            // 上界经动态下压后为严格 LP 松弛天花板，理论上 >= bestLowerBound；
            // 数值抖动可能产生微小负值，用 std::max(0.0, ...) 兜底防负。
            if (bestLowerBound > -DBL_MAX / 2) {
                double denom = std::abs(bestLowerBound) > 1e-9 ? std::abs(bestLowerBound) : 1.0;
                double gap = std::max(0.0, (bestUpperBound - bestLowerBound) / denom);
                finalRelativeGap = gap;
                if (kGapTol > 0.0 && gap <= kGapTol) {
                    prunedNodes += (int)nodeStack.size();
                    solveStatus = SolveStatus::GAP_LIMIT;
                    if (logger) logger->progress(3, "达到目标间隙(gap="
                        + std::to_string(gap) + " <= " + std::to_string(kGapTol)
                        + ")，停止分支");
                    break;
                }
            }

            // 节点限制必须在 pop 前检查，保证所有活节点仍计入有效上界。
            if (totalNodes >= config->max_branch_nodes) {
                prunedNodes += nodeStack.size();
                solveStatus = SolveStatus::NODE_LIMIT;
                if (logger) logger->progress(3, "达到最大节点数限制("
                    + std::to_string(config->max_branch_nodes) + ")，停止分支");
                break;
            }

            // 取出栈顶节点（DFS）
            // dfs preserves historical LIFO behavior. best_bound selects the
            // largest certified LP bound. hybrid switches after the first
            // integer node (root rounding alone does not trigger the switch).
            std::string nodeStrategy =
                config ? config->bb_node_strategy : std::string("dfs");
            double incumbentGap = DBL_MAX;
            if (bestLowerBound > -DBL_MAX / 2
                && bestUpperBound < DBL_MAX / 2) {
                double denom = std::abs(bestLowerBound) > 1e-9
                                   ? std::abs(bestLowerBound) : 1.0;
                incumbentGap =
                    std::max(0.0, (bestUpperBound - bestLowerBound) / denom);
            }
            bool hybridEligible =
                nodeStrategy == "hybrid"
                && integerSolutions > 0
                && incumbentGap <= config->bb_hybrid_switch_gap;
            int dfsQuota = config ? std::max(0, config->bb_hybrid_dfs_quota) : 3;
            bool hybridBestTurn =
                hybridEligible
                && (hybridEligibleSelections % (dfsQuota + 1) == dfsQuota);
            int stagnationNodes =
                config ? std::max(1, config->bb_adaptive_stagnation_nodes) : 12;
            int cooldownNodes =
                config ? std::max(1, config->bb_adaptive_cooldown_nodes) : 12;
            bool adaptiveBestTurn =
                nodeStrategy == "adaptive"
                && integerSolutions > 0
                && incumbentGap <= config->bb_hybrid_switch_gap
                && nodeSelections - lastIncumbentImprovementSelection >= stagnationNodes
                && nodeSelections - lastAdaptiveRestartSelection >= cooldownNodes;
            bool chooseBestBound =
                nodeStrategy == "best_bound" || hybridBestTurn || adaptiveBestTurn;
            if (hybridEligible) {
                hybridEligibleSelections++;
                if (logger && (hybridBestTurn || hybridEligibleSelections == 1)) {
                    logger->log(
                        2,
                        "[BB-NODESEL] strategy=hybrid choice="
                            + std::string(hybridBestTurn ? "best_bound" : "dfs")
                            + " eligibleCount="
                            + std::to_string(hybridEligibleSelections)
                            + " active=" + std::to_string(nodeStack.size())
                            + " gap=" + std::to_string(incumbentGap));
                }
            } else if (nodeStrategy == "hybrid") {
                hybridEligibleSelections = 0;
            }
            if (adaptiveBestTurn) {
                lastAdaptiveRestartSelection = nodeSelections;
                if (logger) {
                    logger->log(
                        2,
                        "[BB-NODESEL] strategy=adaptive choice=best_bound"
                            " selection=" + std::to_string(nodeSelections + 1)
                            + " stagnant="
                            + std::to_string(
                                nodeSelections - lastIncumbentImprovementSelection)
                            + " active=" + std::to_string(nodeStack.size())
                            + " gap=" + std::to_string(incumbentGap));
                }
            }
            size_t selectedNode = nodeStack.size() - 1;
            if (chooseBestBound) {
                for (size_t idx = 0; idx < nodeStack.size(); idx++) {
                    if (nodeStack[idx].cgResult.lpObjective
                        > nodeStack[selectedNode].cgResult.lpObjective) {
                        selectedNode = idx;
                    }
                }
            }
            BnBNode currentNode = nodeStack[selectedNode];
            nodeStack.erase(nodeStack.begin() + selectedNode);
            nodeSelections++;

            // 剪枝检查：如果当前节点的LP解 ≤ bestLowerBound，则剪枝
            if (currentNode.cgResult.lpObjective <= bestLowerBound + 1e-6) {
                prunedNodes++;
                continue;
            }

            // ===== 4. 选择分支变量 + 整数判据（方案 B：统一 x_ij 口径） =====
            // 用同一把尺子(聚合 x_ij ∈ {0,1})判整数并选分支变量，
            // 并跳过已固定的 (j,i) 对，杜绝反复分支导致的节点爆炸。
            int id_j = -1;
            int id_i = -1;
            bool hasFractional = selectBranchingVariable(
                currentNode.cgResult, currentNode.state, id_j, id_i);

            if (!hasFractional) {
                // 所有(未固定的) x_ij 都是整数：当前 LP 解即整数可行解
                if (currentNode.cgResult.lpObjective > bestLowerBound) {
                    double previousLowerBound = bestLowerBound;
                    bestLowerBound = currentNode.cgResult.lpObjective;
                    saveIntegerSolution(currentNode.cgResult);
                    integerSolutions++;
                    lastIncumbentImprovementSelection = nodeSelections;
                    if (logger) {
                        logger->log(
                            2,
                            "[BB-INCUMBENT] selection="
                                + std::to_string(nodeSelections)
                                + " node=" + std::to_string(totalNodes)
                                + " previous=" + std::to_string(previousLowerBound)
                                + " value=" + std::to_string(bestLowerBound)
                                + " active=" + std::to_string(nodeStack.size()));
                    }
                }
                continue;
            }

            totalNodes++;

            // ===== 5. 生成两个子节点：x_ij = 0 和 x_ij = 1 =====
            // [DIAG] 打印分支变量，确认列过滤影响了哪些Fill[i]
            if (logger) {
                logger->log(2, "[BB-DIAG] 分支 x_{dc=" + std::to_string(id_j)
                    + ",retailer=" + std::to_string(id_i) + "}, 节点数=" + std::to_string(totalNodes));
            }
            columnGen.setConsoleProgress(false);

            // 5a. 左分支：x_ij = 0（DC j 禁止服务零售商 i）
            // 只做可行性 + 剪枝判定；整数判定统一交给下一轮循环开头的
            // selectBranchingVariable(x_ij 口径)，避免 childResult.isInteger 的 z 口径埋雷。
            {
                BranchState childState = currentNode.state.branchOnRetailer(id_j, id_i, 0);
                CGResult childResult = columnGen.solve(
                    childState, &currentNode.cgResult, true);

                if (childResult.feasible && !childResult.certifiedOptimal) {
                    solveStatus = SolveStatus::CG_ITERATION_LIMIT;
                    abortedByUncertifiedCG = true;
                } else if (childResult.feasible
                    && childResult.lpObjective > bestLowerBound + 1e-6) {
                    nodeStack.emplace_back(childState, childResult);
                } else {
                    prunedNodes++;
                }
            }
            if (abortedByUncertifiedCG) break;

            // 5b. 右分支：x_ij = 1（DC j 必须服务零售商 i）
            {
                BranchState childState = currentNode.state.branchOnRetailer(id_j, id_i, 1);
                CGResult childResult = columnGen.solve(
                    childState, &currentNode.cgResult, true);

                if (childResult.feasible && !childResult.certifiedOptimal) {
                    solveStatus = SolveStatus::CG_ITERATION_LIMIT;
                    abortedByUncertifiedCG = true;
                } else if (childResult.feasible
                    && childResult.lpObjective > bestLowerBound + 1e-6) {
                    nodeStack.emplace_back(childState, childResult);
                } else {
                    prunedNodes++;
                }
            }
            if (abortedByUncertifiedCG) break;
        }

        auto end = std::chrono::steady_clock::now();

        // 本次 BB 纯分支时间 = 总耗时 - 列生成耗时（CG 是分支过程中的子步骤）
        double totalSec = std::chrono::duration<double>(end - start).count();
        bbTime = std::max(0.0, totalSec - columnGen.getCGTime());

        // 只有搜索树耗尽且所有节点均完成认证，才能声明最优。
        if (nodeStack.empty() && solveStatus == SolveStatus::NOT_STARTED) {
            solveStatus = bestLowerBound > -DBL_MAX / 2
                              ? SolveStatus::OPTIMAL
                              : SolveStatus::NO_INTEGER_SOLUTION;
            if (solveStatus == SolveStatus::OPTIMAL) {
                bestUpperBound = bestLowerBound;
                finalRelativeGap = 0.0;
            }
        }

        // 没找到整数解时绝不返回根节点LP值。
        if (bestLowerBound <= -DBL_MAX / 2) {
            if (solveStatus == SolveStatus::NOT_STARTED) {
                solveStatus = SolveStatus::NO_INTEGER_SOLUTION;
            }
            bestSolution.solveStatus = solveStatus;
            bestSolution.bestBound = bestUpperBound;
            bestSolution.relativeGap = finalRelativeGap;
            bestSolution.hasIntegerSolution = false;
            return -DBL_MAX;
        }

        if (finalRelativeGap == DBL_MAX && bestUpperBound < DBL_MAX / 2) {
            double denom = std::abs(bestLowerBound) > 1e-9
                               ? std::abs(bestLowerBound) : 1.0;
            finalRelativeGap = std::max(
                0.0, (bestUpperBound - bestLowerBound) / denom);
        }
        bestSolution.solveStatus = solveStatus;
        bestSolution.bestBound = bestUpperBound;
        bestSolution.relativeGap = finalRelativeGap;
        bestSolution.hasIntegerSolution = true;

        if (logger) {
            logger->progress(3, "分支定界结束：探索 " + std::to_string(totalNodes)
                + " 节点，剪枝 " + std::to_string(prunedNodes)
                + "，整数解 " + std::to_string(integerSolutions)
                + "，状态=" + std::string(solveStatusName(solveStatus))
                + "，最好可行 obj=" + std::to_string(bestLowerBound)
                + "，gap=" + std::to_string(finalRelativeGap));
        }

        return bestLowerBound;
    }

private:
    double rc_eps = 1.0e-6;

    /**
     * 统一的分支变量选择 / 整数性判据（方案 B 核心）。
     *
     * 【为什么统一】原实现里，CG 用「列变量 z 是否 0/1」判整数(isInteger)，
     * 而 BB 却按「聚合 x_ij = Σ_{k:S_k[i]=1} z_j[k] 是否整数」选分支变量，
     * 两把尺子口径不一致：同一 DC 的两个含零售商 i 的列各取 0.5 时，
     * x_ij=1(看似整数) 但 z 是分数——CG 判非整数、BB 又找不到该对的分数，
     * 导致对已固定的 (j,i) 反复分支、整数解恒为 0、节点爆炸。
     *
     * 【本函数的唯一尺子】只认聚合 x_ij ∈ {0,1}。由于列池已去重
     * (见 CG 的暖启动去重)，「所有 x_ij 均整数」等价于「z 均整数」：
     * 若某 DC 用两不同列各 0.5 拼出某 x_ij=1，则这两列的服务集必有差异，
     * 差异零售商 i' 的 x_i'j 必为分数，会被本函数抓到继续分支。
     *
     * 【收紧保证】跳过已在 branchState 固定过的 (j,i) 对，杜绝对同一变量
     * 反复分支。分支深度上界回落到 numDC × numRetailer，爆炸消除。
     *
     * @param cgResult    当前节点的列生成结果（LP 松弛解）
     * @param branchState 当前节点已累积的分支约束
     * @param out_j       输出：选中的 DC 索引（无可分支变量时为 -1）
     * @param out_i       输出：选中的零售商索引（无可分支变量时为 -1）
     * @return true 表示找到可分支的分数变量；false 表示当前解为整数可行解
     */
    bool selectBranchingVariable(const CGResult& cgResult,
                                 const BranchState& branchState,
                                 int& out_j, int& out_i) const {
        out_j = -1;
        out_i = -1;
        double max_frac = 0.0;

        const auto& z_j = cgResult.final_z_j;
        const auto& j_S = cgResult.j_S;
        for (int j = 0; j < (int)z_j.size(); j++) {
            int numRt = 0;
            for (int k = 0; k < (int)j_S[j].size(); k++) {
                numRt = std::max(numRt, (int)j_S[j][k].size());
            }
            std::vector<double> x_i(numRt, 0.0);
            for (int k = 0; k < (int)z_j[j].size() && k < (int)j_S[j].size(); k++) {
                double zval = z_j[j][k];
                if (zval <= rc_eps) continue;
                const auto& S = j_S[j][k];
                for (int i = 0; i < (int)S.size(); i++) {
                    if (S[i] == 1) x_i[i] += zval;
                }
            }
            for (int i = 0; i < numRt; i++) {
                // 跳过已在上层固定过的 (j,i) 对，保证分支单调收紧、不反复分支
                bool alreadyFixed = false;
                for (const auto& rb : branchState.retailerBranches) {
                    if (rb.dcIndex == j && rb.retailerIndex == i) {
                        alreadyFixed = true;
                        break;
                    }
                }
                if (alreadyFixed) continue;

                double frac = std::abs(x_i[i] - std::round(x_i[i]));
                if (frac > rc_eps && frac > max_frac) {
                    max_frac = frac;
                    out_j = j;
                    out_i = i;
                }
            }
        }
        return out_j != -1;
    }

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

    // 方案B：根节点整数启发式（贪心 rounding）。基于根 LP 分数解 final_z_j
    // 贪心构造可行整数解抬 bestLowerBound：每 DC 至多 1 列、每零售商至多 1 次，
    // 按 z 值降序选列，冲突跳过。只抬下界，不影响最终全局最优性。
    double roundingHeuristic(const CGResult& rootResult) {
        int numRetailer = inst->numRetailer;
        struct Cand { int j; int k; double z; };
        std::vector<Cand> cands;
        for (int j = 0; j < (int)rootResult.final_z_j.size(); j++) {
            for (int k = 0; k < (int)rootResult.final_z_j[j].size(); k++) {
                double zval = rootResult.final_z_j[j][k];
                if (zval > rc_eps) cands.push_back({j, k, zval});
            }
        }
        std::sort(cands.begin(), cands.end(),
                  [](const Cand& a, const Cand& b) { return a.z > b.z; });
        std::vector<int> retailerUsed(numRetailer, 0);
        std::vector<int> dcUsed;
        Solution heuristicSolution;
        double heuristicProfit = 0.0;
        int chosenCols = 0;
        for (const auto& c : cands) {
            bool dcTaken = false;
            for (int dj : dcUsed) if (dj == c.j) { dcTaken = true; break; }
            if (dcTaken) continue;
            const std::vector<int>& S = rootResult.j_S[c.j][c.k];
            bool conflict = false;
            for (int i = 0; i < (int)S.size() && i < numRetailer; i++)
                if (S[i] == 1 && retailerUsed[i] == 1) { conflict = true; break; }
            if (conflict) continue;
            for (int i = 0; i < (int)S.size() && i < numRetailer; i++)
                if (S[i] == 1) retailerUsed[i] = 1;
            dcUsed.push_back(c.j);
            double pj = rootResult.p_j[c.j][c.k];
            double colProfit = objFormula.columnProfit(
                *inst, c.j, S, pj, inst->w[c.j]);
            heuristicProfit += colProfit;
            DCSolution dcSol;
            dcSol.dcIndex = c.j;
            dcSol.w = inst->w[c.j];
            dcSol.p = pj;
            dcSol.S = S;
            dcSol.profit = colProfit;
            heuristicSolution.dcSolutions.push_back(dcSol);
            chosenCols++;
        }
        if (chosenCols == 0) return -DBL_MAX;
        heuristicSolution.totalProfit = heuristicProfit;
        heuristicSolution.w = inst->w;
        heuristicSolution.hasIntegerSolution = true;
        bestSolution = heuristicSolution;
        integerSolutions++;
        return heuristicProfit;
    }

    // 在根节点严格列生成已经得到的有限列池上求解一个短时 0-1 restricted master。
    // 其解由原问题的合法列组成，故始终是原整数主问题的可行解；只用于抬高下界，
    // 不参与上界或最优性认证，时间到仍不影响 Branch-and-Price 的精确性。
    double restrictedMasterMIPHeuristic(const CGResult& rootResult) {
        if (!inst) return -DBL_MAX;
        IloEnv env;
        try {
            IloModel model(env);
            IloObjective objective = IloAdd(model, IloMaximize(env));
            IloRangeArray retailerRows(
                env, inst->numRetailer, -IloInfinity, 1.0);
            IloRangeArray dcRows(
                env, inst->numDC, -IloInfinity, 1.0);
            model.add(retailerRows);
            model.add(dcRows);

            IloArray<IloNumVarArray> y(env, inst->numDC);
            for (int j = 0; j < inst->numDC; j++) {
                y[j] = IloNumVarArray(env);
                if (j >= (int)rootResult.j_S.size()) continue;
                for (int k = 0; k < (int)rootResult.j_S[j].size(); k++) {
                    const auto& S = rootResult.j_S[j][k];
                    double pj = (j < (int)rootResult.p_j.size()
                                 && k < (int)rootResult.p_j[j].size())
                                    ? rootResult.p_j[j][k] : 0.0;
                    double profit = objFormula.columnProfit(
                        *inst, j, S, pj, inst->w[j]);
                    IloNumColumn col = objective(profit) + dcRows[j](1.0);
                    for (int i = 0; i < inst->numRetailer && i < (int)S.size(); i++) {
                        if (S[i] == 1) col += retailerRows[i](1.0);
                    }
                    y[j].add(IloNumVar(col, 0.0, 1.0, ILOBOOL));
                }
            }

            IloCplex cplex(model);
            cplex.setOut(env.getNullStream());
            cplex.setWarning(env.getNullStream());
            cplex.setParam(IloCplex::Threads, 1);
            double timeLimit = config
                ? std::max(0.01, config->root_rmp_mip_time_limit_sec) : 5.0;
            cplex.setParam(IloCplex::TiLim, timeLimit);
            cplex.setParam(IloCplex::MIPDisplay, 0);

            if (!cplex.solve()
                || !(cplex.getStatus() == IloAlgorithm::Optimal
                     || cplex.getStatus() == IloAlgorithm::Feasible)) {
                env.end();
                return -DBL_MAX;
            }

            double profit = cplex.getObjValue();
            if (profit <= bestLowerBound + rc_eps) {
                env.end();
                return profit;
            }

            Solution candidate;
            candidate.totalProfit = profit;
            candidate.w = inst->w;
            candidate.hasIntegerSolution = true;
            std::vector<int> served(inst->numRetailer, 0);
            for (int j = 0; j < inst->numDC; j++) {
                for (int k = 0; k < y[j].getSize(); k++) {
                    if (cplex.getValue(y[j][k]) <= 0.5) continue;
                    DCSolution dcSol;
                    dcSol.dcIndex = j;
                    dcSol.w = inst->w[j];
                    dcSol.p = rootResult.p_j[j][k];
                    dcSol.S = rootResult.j_S[j][k];
                    dcSol.profit = objFormula.columnProfit(
                        *inst, j, dcSol.S, dcSol.p, dcSol.w);
                    candidate.dcSolutions.push_back(dcSol);
                    candidate.numDCsOpen++;
                    for (int i = 0; i < inst->numRetailer
                                    && i < (int)dcSol.S.size(); i++) {
                        if (dcSol.S[i] == 1 && !served[i]) {
                            served[i] = 1;
                            candidate.numRtsServed++;
                        }
                    }
                }
            }
            bestSolution = candidate;
            integerSolutions++;
            env.end();
            return profit;
        } catch (const IloException& e) {
            if (logger) logger->log(
                1, std::string("[BB-ROOT-RMIP] CPLEX exception: ") + e.getMessage());
            env.end();
            return -DBL_MAX;
        } catch (...) {
            if (logger) logger->log(1, "[BB-ROOT-RMIP] unknown exception");
            env.end();
            return -DBL_MAX;
        }
    }
};

#endif // BRANCH_AND_BOUND_H
