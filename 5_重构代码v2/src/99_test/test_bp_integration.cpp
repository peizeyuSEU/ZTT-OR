#include "../10_common/Config.h"
#include "../10_common/Instance.h"
#include "../7_formula/01_Revenue.h"
#include "../7_formula/08_Objective.h"
#include "../8_solver/CplexWrapper.h"
#include "../8_solver/03_BranchAndBound.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expectNear(double actual, double expected, const std::string& label,
                double tolerance = 1e-7) {
    const double scale = std::max(
        1.0, std::max(std::abs(actual), std::abs(expected)));
    if (std::abs(actual - expected) > tolerance * scale) {
        throw std::runtime_error(
            label + ": actual=" + std::to_string(actual)
            + ", expected=" + std::to_string(expected));
    }
}

Instance makeSmallInstance() {
    Instance inst;
    inst.numDC = 2;
    inst.numRetailer = 3;
    inst.mu = {100, 130, 170};
    inst.variance = {4.0, 9.0, 16.0};
    inst.f = {500.0, 550.0};
    inst.fc = {20.0, 24.0};
    inst.F = {10.0, 12.0};
    inst.g = {15.0, 17.0};
    inst.L = {1.0, 1.2};
    inst.h = 2.0;
    inst.p = 2.0;
    inst.hat_h = 0.5;
    inst.a_dist = {15.0, 25.0};
    inst.dist = {{10.0, 25.0}, {20.0, 15.0}, {30.0, 10.0}};
    inst.b = {0.02, 0.03};
    inst.bb = {{0.03, 0.07}, {0.05, 0.03}, {0.08, 0.02}};
    inst.reservePrice = {70.0, 80.0, 90.0};
    inst.delta = 25.0;
    inst.beta = {200.0, 250.0};
    inst.w = {0.25, 0.6};
    inst.z_alpha = 1.96;
    return inst;
}

Config exactConfig() {
    Config cfg;
    cfg.num_dc = 2;
    cfg.num_retailers = 3;
    cfg.run_mode = "fixed_w";
    cfg.fixed_w = {0.25, 0.6};
    cfg.use_sqrt_investment = true;
    cfg.investment_exponent = 0.5;
    cfg.use_invest_in_column = true;
    cfg.pricing_algorithm = 1;
    cfg.pricing_max_cols_per_dc = 1;
    cfg.parallel_pricing = false;
    cfg.dual_smooth_alpha = 0.0;
    cfg.cg_early_stop = false;
    cfg.max_cg_iterations = 10000;
    cfg.max_branch_nodes = 10000;
    cfg.bp_time_limit_sec = 30.0;
    cfg.bp_relative_gap = 0.0;
    cfg.root_heuristic = false;
    cfg.root_rmp_mip_heuristic = false;
    cfg.rc_eps = 1e-7;
    cfg.validateOrThrow();
    return cfg;
}

double solveCompleteIntegerMaster(const Instance& inst, const Config& cfg) {
    IloEnv env;
    try {
        IloModel model(env);
        IloObjective objective = IloAdd(model, IloMaximize(env));
        IloRangeArray retailerRows(env, inst.numRetailer, -IloInfinity, 1.0);
        IloRangeArray dcRows(env, inst.numDC, -IloInfinity, 1.0);
        model.add(retailerRows);
        model.add(dcRows);
        IloNumVarArray columnVariables(env);

        ObjectiveFormula formula(cfg);
        for (int j = 0; j < inst.numDC; ++j) {
            for (double price : inst.reservePrice) {
                for (int mask = 1; mask < (1 << inst.numRetailer); ++mask) {
                    std::vector<int> serviceSet(inst.numRetailer, 0);
                    for (int i = 0; i < inst.numRetailer; ++i) {
                        serviceSet[i] = (mask >> i) & 1;
                    }
                    if (!RevenueFormula::serviceSetAcceptsPrice(
                            inst, serviceSet, price)) {
                        continue;
                    }
                    IloNumColumn col = objective(
                        formula.columnProfit(inst, j, serviceSet, price, inst.w[j]));
                    col += dcRows[j](1.0);
                    for (int i = 0; i < inst.numRetailer; ++i) {
                        if (serviceSet[i] == 1) col += retailerRows[i](1.0);
                    }
                    // Keep the column variables alive for the complete MIP,
                    // just as ColumnGeneration retains its z_j arrays.
                    columnVariables.add(IloNumVar(col, 0.0, 1.0, ILOBOOL));
                }
            }
        }

        IloCplex cplex(model);
        cplex.setOut(env.getNullStream());
        cplex.setWarning(env.getNullStream());
        cplex.setParam(IloCplex::Threads, 1);
        if (!cplex.solve()
            || !CplexEnv::isOptimalStatus(cplex.getStatus())) {
            throw std::runtime_error("complete integer master was not optimal");
        }
        const double objectiveValue = cplex.getObjValue();
        env.end();
        return objectiveValue;
    } catch (...) {
        try {
            env.end();
        } catch (...) {
        }
        throw;
    }
}

}  // namespace

int main() {
    try {
        if (!CplexEnv::isOptimalStatus(IloAlgorithm::Optimal)
            || CplexEnv::isOptimalStatus(IloAlgorithm::Feasible)) {
            throw std::runtime_error("RMP optimal-status gate is incorrect");
        }

        const Config cfg = exactConfig();
        const Instance instance = makeSmallInstance();

        // End-to-end certification check: full enumerated IP versus the actual
        // Algorithm-3 + CG + B&B implementation on the same fixed-w instance.
        const double fullMasterObjective = solveCompleteIntegerMaster(instance, cfg);
        BranchAndBound exactBP;
        exactBP.setInstance(instance);
        exactBP.setConfig(cfg);
        const double bpObjective = exactBP.solve();
        if (exactBP.getSolveStatus() != SolveStatus::OPTIMAL
            || !exactBP.getBestSolution().hasIntegerSolution) {
            throw std::runtime_error("BP did not certify the small integration instance");
        }
        expectNear(bpObjective, fullMasterObjective,
                   "complete master versus branch-and-price");

        // The root deadline may occur before any RMP solve. The all-zero column
        // selection remains a valid integer incumbent with objective zero.
        Config immediateTimeout = cfg;
        immediateTimeout.bp_time_limit_sec = 1e-12;
        BranchAndBound timeoutBP;
        timeoutBP.setInstance(instance);
        timeoutBP.setConfig(immediateTimeout);
        const double timeoutObjective = timeoutBP.solve();
        if (timeoutBP.getSolveStatus() != SolveStatus::TIME_LIMIT
            || !timeoutBP.getBestSolution().hasIntegerSolution
            || !timeoutBP.getBestSolution().dcSolutions.empty()) {
            throw std::runtime_error(
                "root timeout did not retain the empty integer incumbent");
        }
        expectNear(timeoutObjective, 0.0, "root timeout empty incumbent");

        // When every non-empty column loses money, the certified optimum is the
        // empty solution; rounding must not replace it with a negative column.
        Instance negativeInstance = instance;
        negativeInstance.f = {1.0e8, 1.0e8};
        BranchAndBound negativeBP;
        negativeBP.setInstance(negativeInstance);
        negativeBP.setConfig(cfg);
        const double negativeObjective = negativeBP.solve();
        if (negativeBP.getSolveStatus() != SolveStatus::OPTIMAL
            || !negativeBP.getBestSolution().hasIntegerSolution
            || !negativeBP.getBestSolution().dcSolutions.empty()) {
            throw std::runtime_error(
                "negative-column instance did not keep the empty optimum");
        }
        expectNear(negativeObjective, 0.0, "empty optimum with negative columns");

        std::cout << "PASS: BP integration, empty incumbent, and RMP status gate.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
