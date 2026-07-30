#ifndef TYPES_H
#define TYPES_H

#include <vector>
#include <string>
#include <limits>
#include <cfloat>

enum class SolveStatus {
    NOT_STARTED,
    OPTIMAL,
    FEASIBLE_NOT_PROVEN,
    INFEASIBLE,
    NO_INTEGER_SOLUTION,
    TIME_LIMIT,
    NODE_LIMIT,
    GAP_LIMIT,
    CG_ITERATION_LIMIT,
    ERROR
};

inline const char* solveStatusName(SolveStatus status) {
    switch (status) {
        case SolveStatus::NOT_STARTED: return "NOT_STARTED";
        case SolveStatus::OPTIMAL: return "OPTIMAL";
        case SolveStatus::FEASIBLE_NOT_PROVEN: return "FEASIBLE_NOT_PROVEN";
        case SolveStatus::INFEASIBLE: return "INFEASIBLE";
        case SolveStatus::NO_INTEGER_SOLUTION: return "NO_INTEGER_SOLUTION";
        case SolveStatus::TIME_LIMIT: return "TIME_LIMIT";
        case SolveStatus::NODE_LIMIT: return "NODE_LIMIT";
        case SolveStatus::GAP_LIMIT: return "GAP_LIMIT";
        case SolveStatus::CG_ITERATION_LIMIT: return "CG_ITERATION_LIMIT";
        case SolveStatus::ERROR: return "ERROR";
    }
    return "ERROR";
}

struct PricingResult {
    double reducedCost = -DBL_MAX;
    std::vector<int> optimal_S;
    double optimal_pj = 0.0;
    int dcIndex = -1;
    PricingResult() = default;
    PricingResult(int numRetailers) : optimal_S(numRetailers, 0) {}
};

struct CGResult {
    double lpObjective = 0.0;
    bool isInteger = false;
    bool feasible = true;
    bool certifiedOptimal = false;
    bool iterationLimitReached = false;
    std::vector<std::vector<double>> final_z_j;
    std::vector<std::vector<std::vector<int>>> j_S;
    std::vector<std::vector<double>> p_j;
    int numColumns = 0;
};

/**
 * 分支约束：对 DC-零售商对 (dcIndex, retailerIndex) 的分配变量 x_ij 分支。
 *
 * x_ij = Σ_{k: S_k[i]=1} z_j[k]，表示零售商 i 是否由 DC j 服务。
 *
 * value = 1：DC j 必须服务零售商 i（该 DC 上仅允许 S[i]=1 的列）
 * value = 0：DC j 禁止服务零售商 i（该 DC 上仅允许 S[i]=0 的列）
 *
 * 用稳定的 (dcIndex, retailerIndex) 标识变量，而非易变的列池位置索引，
 * 从根本上保证分支约束在子节点重建列池后仍然有效。
 */
struct RetailerBranch {
    int dcIndex = -1;       // DC 索引 j
    int retailerIndex = -1;  // 零售商索引 i
    int value = 0;           // 0 或 1
};

struct BranchState {
    std::vector<RetailerBranch> retailerBranches;
    BranchState() = default;

    /** 派生子节点：在当前约束基础上追加一条 (j, i, value) 约束 */
    BranchState branchOnRetailer(int dcIndex, int retailerIndex, int value) const {
        BranchState child;
        child.retailerBranches = retailerBranches;
        RetailerBranch rb;
        rb.dcIndex = dcIndex;
        rb.retailerIndex = retailerIndex;
        rb.value = value;
        child.retailerBranches.push_back(rb);
        return child;
    }

    /**
     * 判断某个 DC j 的服务集合 S 是否满足当前所有分支约束。
     * 用于初始列过滤和定价列过滤。
     */
    bool isColumnFeasible(int dcIndex, const std::vector<int>& S) const {
        for (const auto& rb : retailerBranches) {
            if (rb.dcIndex != dcIndex) continue;
            int served = (rb.retailerIndex < (int)S.size())
                             ? S[rb.retailerIndex] : 0;
            if (rb.value == 1 && served != 1) return false;  // 必须服务但未服务
            if (rb.value == 0 && served != 0) return false;  // 禁止服务但服务了
        }
        return true;
    }
};

struct DCSolution {
    int dcIndex = -1;
    double w = 0.0;
    double p = 0.0;
    std::vector<int> S;
    double profit = 0.0;
};

struct Solution {
    std::vector<DCSolution> dcSolutions;
    double totalProfit = 0.0;
    double totalInvestmentCost = 0.0;
    int numDCsOpen = 0;
    int numRtsServed = 0;
    std::vector<double> w;
    SolveStatus solveStatus = SolveStatus::NOT_STARTED;
    double bestBound = DBL_MAX;
    double relativeGap = DBL_MAX;
    bool hasIntegerSolution = false;
    Solution() = default;
};

struct ConvergencePoint {
    int generation;
    double bestFitness;
    double avgFitness;
    double worstFitness;
    double elapsedTime;
};

#endif
