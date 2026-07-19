#ifndef TYPES_H
#define TYPES_H

#include <vector>
#include <string>
#include <limits>
#include <cfloat>

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
    std::vector<std::vector<double>> final_z_j;
    std::vector<std::vector<std::vector<int>>> j_S;
    std::vector<std::vector<double>> p_j;
    int numColumns = 0;
};

struct BranchState {
    std::vector<int> branchId;
    std::vector<int> lb;
    std::vector<int> ub;
    int addSize = 0;
    BranchState() = default;
    BranchState branch(int id_j, int id_S, int value) const {
        BranchState child;
        child.addSize = addSize + 1;
        child.branchId = branchId;
        child.branchId.push_back(id_j);
        child.branchId.push_back(id_S);
        child.lb = lb;
        child.lb.push_back(value);
        child.ub = ub;
        child.ub.push_back(value);
        return child;
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
