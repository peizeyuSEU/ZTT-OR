#include "../10_common/Config.h"
#include "../10_common/ExactWKey.h"
#include "../10_common/GAFitnessUtils.h"
#include "../10_common/Instance.h"
#include "../5_result/ExperimentResult.h"
#include "../7_formula/01_Revenue.h"
#include "../7_formula/02_FacilityCost.h"
#include "../7_formula/03_TransportCost.h"
#include "../7_formula/04_InventoryCost.h"
#include "../7_formula/05_InvestmentCost.h"
#include "../7_formula/08_Objective.h"
#include "../8_solver/01_PricingSolver.h"

#include <cfloat>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expectNear(double actual, double expected, const std::string& label,
                double tolerance = 1e-8) {
    double scale = std::max(1.0, std::max(std::abs(actual), std::abs(expected)));
    if (std::abs(actual - expected) > tolerance * scale) {
        throw std::runtime_error(label + ": actual=" + std::to_string(actual)
                                 + ", expected=" + std::to_string(expected));
    }
}

Instance makeInstance() {
    Instance inst;
    inst.numDC = 1;
    inst.numRetailer = 3;
    inst.mu = {100, 400, 900};
    inst.variance = {4.0, 9.0, 16.0};
    inst.f = {120.0};
    inst.fc = {12.0};
    inst.F = {10.0};
    inst.g = {20.0};
    inst.L = {1.0};
    inst.h = 2.0;
    inst.p = 3.0;
    inst.hat_h = 0.5;
    inst.a_dist = {50.0};
    inst.dist = {{20.0}, {40.0}, {80.0}};
    inst.b = {0.2};
    inst.bb = {{0.1}, {0.15}, {0.25}};
    inst.reservePrice = {80.0, 80.0, 80.0};
    inst.delta = 70.0;
    inst.beta = {500.0};
    inst.w = {0.6};
    inst.z_alpha = 1.96;
    return inst;
}

Instance makeRandomInstance(int n, std::mt19937& rng) {
    std::uniform_real_distribution<double> demand(20.0, 500.0);
    std::uniform_real_distribution<double> variance(1.0, 40.0);
    std::uniform_real_distribution<double> distance(5.0, 100.0);
    std::uniform_real_distribution<double> reserve(45.0, 105.0);
    std::uniform_real_distribution<double> coefficient(0.02, 0.30);

    Instance inst;
    inst.numDC = 1;
    inst.numRetailer = n;
    inst.mu.resize(n);
    inst.variance.resize(n);
    inst.dist.assign(n, std::vector<double>(1));
    inst.bb.assign(n, std::vector<double>(1));
    inst.reservePrice.resize(n);
    for (int i = 0; i < n; ++i) {
        inst.mu[i] = demand(rng);
        inst.variance[i] = variance(rng);
        inst.dist[i][0] = distance(rng);
        inst.bb[i][0] = coefficient(rng);
        inst.reservePrice[i] = reserve(rng);
    }
    inst.f = {100.0};
    inst.fc = {8.0};
    inst.F = {12.0};
    inst.g = {18.0};
    inst.L = {1.5};
    inst.h = 1.8;
    inst.p = 2.2;
    inst.hat_h = 0.4;
    inst.a_dist = {distance(rng)};
    inst.b = {coefficient(rng)};
    inst.delta = 100.0;
    inst.beta = {400.0};
    inst.w = {0.5};
    inst.z_alpha = 1.96;
    return inst;
}

double manualBaseProfit(const Instance& inst, const std::vector<int>& S,
                        double price, double w) {
    return RevenueFormula::totalRevenue(inst, 0, S, price)
         - FacilityCostFormula::totalFixedCost(inst, 0, w)
         - TransportCostFormula::totalTransportCost(inst, 0, S, w)
         - InventoryCostFormula::totalInventoryCost(inst, 0, S, w);
}

PricingResult bruteForcePricing(const Instance& inst, const Config& config,
                                int j, const std::vector<double>& dual,
                                const BranchState* branch = nullptr) {
    ObjectiveFormula objective(config);
    PricingResult best(inst.numRetailer);
    best.dcIndex = j;
    best.reducedCost = -DBL_MAX;

    for (double price : inst.reservePrice) {
        for (int mask = 0; mask < (1 << inst.numRetailer); ++mask) {
            std::vector<int> S(inst.numRetailer, 0);
            bool priceFeasible = true;
            for (int i = 0; i < inst.numRetailer; ++i) {
                S[i] = (mask >> i) & 1;
                if (S[i] && price > inst.reservePrice[i]) {
                    priceFeasible = false;
                }
            }
            if (!priceFeasible) continue;
            if (branch && !branch->isColumnFeasible(j, S)) continue;

            double rc = objective.columnProfit(
                inst, j, S, price, inst.w[j]);
            for (int i = 0; i < inst.numRetailer; ++i) {
                if (S[i]) rc -= dual[i];
            }
            rc -= dual[inst.numRetailer + j];
            if (rc > best.reducedCost) {
                best.reducedCost = rc;
                best.optimal_S = S;
                best.optimal_pj = price;
            }
        }
    }
    return best;
}

std::vector<PricingResult> bruteBestColumnPerPrice(
        const Instance& inst, const Config& config, int j,
        const std::vector<double>& dual, int maxCols) {
    ObjectiveFormula objective(config);
    std::vector<double> prices = inst.reservePrice;
    std::sort(prices.begin(), prices.end());
    prices.erase(std::unique(prices.begin(), prices.end()), prices.end());

    std::vector<PricingResult> candidates;
    for (double price : prices) {
        PricingResult best(inst.numRetailer);
        best.dcIndex = j;
        best.optimal_pj = price;
        best.reducedCost = -DBL_MAX;
        for (int mask = 0; mask < (1 << inst.numRetailer); ++mask) {
            std::vector<int> S(inst.numRetailer, 0);
            for (int i = 0; i < inst.numRetailer; ++i) {
                S[i] = (mask >> i) & 1;
            }
            if (!RevenueFormula::serviceSetAcceptsPrice(inst, S, price)) continue;
            double rc = objective.columnProfit(inst, j, S, price, inst.w[j]);
            for (int i = 0; i < inst.numRetailer; ++i) {
                if (S[i]) rc -= dual[i];
            }
            rc -= dual[inst.numRetailer + j];
            if (rc > best.reducedCost) {
                best.reducedCost = rc;
                best.optimal_S = S;
            }
        }
        if (best.reducedCost <= config.rc_eps) continue;
        bool merged = false;
        for (auto& existing : candidates) {
            if (existing.optimal_S == best.optimal_S) {
                if (best.reducedCost > existing.reducedCost) existing = best;
                merged = true;
                break;
            }
        }
        if (!merged) candidates.push_back(best);
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const PricingResult& a, const PricingResult& b) {
                  return a.reducedCost > b.reducedCost;
              });
    if ((int)candidates.size() > maxCols) candidates.resize(maxCols);
    return candidates;
}

}  // namespace

int main() {
    try {
        if (std::string(solveOutcomeName(SolveStatus::OPTIMAL, true))
                != "OPTIMAL_CERTIFIED"
            || std::string(solveOutcomeName(SolveStatus::TIME_LIMIT, true))
                != "TIME_LIMIT_WITH_INCUMBENT"
            || std::string(solveOutcomeName(SolveStatus::TIME_LIMIT, false))
                != "TIME_LIMIT_NO_INCUMBENT"
            || std::string(solveOutcomeName(
                   SolveStatus::CG_ITERATION_LIMIT, false))
                != "INCOMPLETE_CG_NO_INCUMBENT") {
            throw std::runtime_error("solve outcome classification failed");
        }

        ExperimentResult gaResult;
        gaResult.runMode = "ga";
        gaResult.solveStatus = SolveStatus::OPTIMAL;
        gaResult.hasIntegerSolution = true;
        if (overallOutcomeName(gaResult) != "HEURISTIC_BEST_FOUND") {
            throw std::runtime_error(
                "GA overall outcome was confused with inner-BP optimality");
        }
        gaResult.hasIntegerSolution = false;
        if (overallOutcomeName(gaResult)
            != "NO_FEASIBLE_SOLUTION_FOUND") {
            throw std::runtime_error(
                "GA without an incumbent received an incorrect outcome");
        }
        ExperimentResult fixedWResult;
        fixedWResult.runMode = "fixed_w";
        fixedWResult.solveStatus = SolveStatus::OPTIMAL;
        fixedWResult.hasIntegerSolution = true;
        if (overallOutcomeName(fixedWResult) != "OPTIMAL_CERTIFIED") {
            throw std::runtime_error(
                "fixed-w optimal certification was not preserved");
        }

        const std::vector<double> partialFailureFitness = {
            100.0, -DBL_MAX, 200.0,
            std::numeric_limits<double>::quiet_NaN()};
        const std::vector<double> partialWeights =
            GAFitnessUtils::selectionWeights(partialFailureFitness);
        if (partialWeights.size() != 4
            || partialWeights[0] != 0.0
            || partialWeights[1] != 0.0
            || partialWeights[2] != 1.0
            || partialWeights[3] != 0.0) {
            throw std::runtime_error(
                "failed GA fitness values received selection weight");
        }
        const std::vector<double> equalValidWeights =
            GAFitnessUtils::selectionWeights(
                {123.0, -DBL_MAX, 123.0});
        if (equalValidWeights[0] != 1.0
            || equalValidWeights[1] != 0.0
            || equalValidWeights[2] != 1.0) {
            throw std::runtime_error(
                "equal valid GA fitness values were not selected uniformly");
        }
        const std::vector<double> noValidWeights =
            GAFitnessUtils::selectionWeights(
                {-DBL_MAX, std::numeric_limits<double>::infinity()});
        if (noValidWeights[0] != 0.0 || noValidWeights[1] != 0.0) {
            throw std::runtime_error(
                "an all-failed GA generation received selection weight");
        }

        const std::vector<double> exactW = {0.5, 0.0};
        const std::vector<double> adjacentW = {
            std::nextafter(0.5, 1.0), 0.0};
        if (ExactWKey::make(exactW) == ExactWKey::make(adjacentW)) {
            throw std::runtime_error(
                "exact w cache key merged adjacent double values");
        }
        if (ExactWKey::make(exactW) != ExactWKey::make(exactW)
            || ExactWKey::make({0.0}) != ExactWKey::make({-0.0})) {
            throw std::runtime_error(
                "exact w cache key is not deterministic");
        }

        const Instance inst = makeInstance();
        const double price = 60.0;
        const double w = inst.w[0];
        const std::vector<int> S = {1, 0, 1};
        const double demand = 1000.0;
        const double investment =
            0.5 * inst.delta * w * w * std::sqrt(demand);
        const double expected = manualBaseProfit(inst, S, price, w) - investment;

        Config paper;
        paper.use_sqrt_investment = true;
        paper.use_invest_in_column = true;
        ObjectiveFormula paperObjective(paper);

        expectNear(paperObjective.columnProfit(inst, 0, S, price, w), expected,
                   "paper column profit");
        expectNear(paperObjective.totalProfit(inst, 0, S, price, w, true), expected,
                   "paper total profit must not double deduct");
        expectNear(paperObjective.getRjs(inst, 0, S, price, w), expected,
                   "paper Rjs");

        Config legacy = paper;
        legacy.use_invest_in_column = false;
        ObjectiveFormula legacyObjective(legacy);
        expectNear(legacyObjective.columnProfit(inst, 0, S, price, w),
                   expected + investment, "legacy column profit");
        expectNear(legacyObjective.totalProfit(inst, 0, S, price, w, true),
                   expected, "legacy complete profit");

        // Exhaust every service set: the paper column coefficient must equal
        // the manuscript formula for all S, including the empty set.
        for (int mask = 0; mask < (1 << inst.numRetailer); ++mask) {
            std::vector<int> candidate(inst.numRetailer, 0);
            for (int i = 0; i < inst.numRetailer; ++i) {
                candidate[i] = (mask >> i) & 1;
            }
            double D = InventoryCostFormula::totalDemand(inst, 0, candidate);
            double direct = manualBaseProfit(inst, candidate, price, w)
                          - 0.5 * inst.delta * w * w * std::sqrt(D);
            expectNear(paperObjective.columnProfit(
                           inst, 0, candidate, price, w),
                       direct, "enumerated manuscript coefficient");
        }

        // Construct a transparent counterexample: legacy ranking prefers the
        // large set, while the complete paper objective prefers the small set.
        const double baseSmall = 100.0;
        const double baseLarge = 104.0;
        const double costSmall = 0.5 * 10.0 * std::sqrt(1.0);
        const double costLarge = 0.5 * 10.0 * std::sqrt(9.0);
        if (!(baseLarge > baseSmall
              && baseLarge - costLarge < baseSmall - costSmall)) {
            throw std::runtime_error("legacy-ranking counterexample failed");
        }

        // Compare every pricing implementation against complete enumeration.
        // This checks the actual RC path used to generate columns, not only the
        // formula helper.
        const std::vector<double> dual = {13.0, 29.0, 7.0, 41.0};
        PricingResult brute = bruteForcePricing(inst, paper, 0, dual);
        for (int algorithm = 0; algorithm <= 2; ++algorithm) {
            Config pricingConfig = paper;
            pricingConfig.pricing_algorithm = algorithm;
            pricingConfig.rc_eps = 1e-9;
            PricingSolver solver;
            solver.setInstance(inst);
            solver.setConfig(pricingConfig);
            PricingResult actual = solver.solve(0, dual);
            expectNear(actual.reducedCost, brute.reducedCost,
                       "pricing algorithm " + std::to_string(algorithm)
                           + " versus exhaustive RC",
                       1e-7);
        }

        // The generalized manuscript model must use the same gamma in the
        // objective and every pricing implementation.
        for (double gamma : {0.25, 0.5, 0.75, 1.0}) {
            Config gammaConfig = paper;
            gammaConfig.investment_exponent = gamma;
            ObjectiveFormula gammaObjective(gammaConfig);
            double gammaInvestment =
                0.5 * inst.delta * w * w * std::pow(demand, gamma);
            expectNear(gammaObjective.columnProfit(inst, 0, S, price, w),
                       manualBaseProfit(inst, S, price, w) - gammaInvestment,
                       "generalized investment exponent");
            PricingResult gammaBrute =
                bruteForcePricing(inst, gammaConfig, 0, dual);
            for (int algorithm = 0; algorithm <= 2; ++algorithm) {
                gammaConfig.pricing_algorithm = algorithm;
                PricingSolver solver;
                solver.setInstance(inst);
                solver.setConfig(gammaConfig);
                PricingResult actual = solver.solve(0, dual);
                expectNear(actual.reducedCost, gammaBrute.reducedCost,
                           "gamma pricing algorithm "
                               + std::to_string(algorithm),
                           1e-7);
            }
        }

        // Deterministic randomized exhaustive regression.  This is broad
        // numerical evidence, not a substitute for a theoretical proof.
        std::mt19937 rng(20260730);
        std::uniform_real_distribution<double> dualDist(-20000.0, 50000.0);
        const double gammaValues[] = {0.25, 0.5, 0.75, 1.0};
        int randomizedCases = 0;
        for (int n = 4; n <= 12; ++n) {
            for (int trial = 0; trial < 25; ++trial) {
                Instance randomInst = makeRandomInstance(n, rng);
                randomInst.w[0] =
                    0.05 + 0.90 * static_cast<double>(trial % 10) / 9.0;
                Config randomConfig = paper;
                randomConfig.investment_exponent =
                    gammaValues[(n + trial) % 4];
                randomConfig.rc_eps = 1e-9;
                std::vector<double> randomDual(n + 1);
                for (double& value : randomDual) value = dualDist(rng);

                BranchState randomBranch;
                const int branchMode = trial % 4;
                if (branchMode == 1) {
                    randomBranch =
                        randomBranch.branchOnRetailer(0, 0, 0);
                } else if (branchMode == 2) {
                    randomBranch =
                        randomBranch.branchOnRetailer(0, 0, 1);
                } else if (branchMode == 3) {
                    randomBranch =
                        randomBranch.branchOnRetailer(0, 0, 1);
                    randomBranch =
                        randomBranch.branchOnRetailer(0, 1, 0);
                    randomBranch =
                        randomBranch.branchOnRetailer(0, 2, 1);
                }
                const BranchState* branch =
                    branchMode == 0 ? nullptr : &randomBranch;
                PricingResult exhaustive = bruteForcePricing(
                    randomInst, randomConfig, 0, randomDual, branch);
                for (int algorithm = 0; algorithm <= 2; ++algorithm) {
                    randomConfig.pricing_algorithm = algorithm;
                    PricingSolver solver;
                    solver.setInstance(randomInst);
                    solver.setConfig(randomConfig);
                    PricingResult actual =
                        solver.solve(0, randomDual, branch);
                    double scale = std::max(
                        1.0, std::max(std::abs(actual.reducedCost),
                                      std::abs(exhaustive.reducedCost)));
                    if (std::abs(actual.reducedCost
                                 - exhaustive.reducedCost) > 1e-7 * scale) {
                        ObjectiveFormula diagnosticObjective(randomConfig);
                        double recomputed = diagnosticObjective.columnProfit(
                            randomInst, 0, actual.optimal_S,
                            actual.optimal_pj, randomInst.w[0]);
                        for (int i = 0; i < n; ++i) {
                            if (actual.optimal_S[i]) recomputed -= randomDual[i];
                        }
                        recomputed -= randomDual[n];
                        std::ostringstream message;
                        message << "random exhaustive mismatch: n=" << n
                                << ", trial=" << trial
                                << ", algorithm=" << algorithm
                                << ", gamma="
                                << randomConfig.investment_exponent
                                << ", actual=" << actual.reducedCost
                                << ", exhaustive="
                                << exhaustive.reducedCost
                                << ", exhaustiveP="
                                << exhaustive.optimal_pj
                                << ", exhaustiveS=";
                        for (int served : exhaustive.optimal_S) {
                            message << served;
                        }
                        message
                                << ", recomputed=" << recomputed
                                << ", p=" << actual.optimal_pj
                                << ", S=";
                        for (int served : actual.optimal_S) message << served;
                        message << ", branchFeasible="
                                << (!branch
                                    || branch->isColumnFeasible(
                                           0, actual.optimal_S))
                                << ", priceFeasible="
                                << RevenueFormula::serviceSetAcceptsPrice(
                                       randomInst, actual.optimal_S,
                                       actual.optimal_pj);
                        throw std::runtime_error(message.str());
                    }
                }
                ++randomizedCases;
            }
        }

        // Right branches force both retailers into the same service set.  With
        // reserve prices 60 and 80, every feasible common price must be <= 60.
        Instance branchedInst = inst;
        branchedInst.reservePrice = {60.0, 80.0, 100.0};
        BranchState rightBranches;
        rightBranches = rightBranches.branchOnRetailer(0, 0, 1);
        rightBranches = rightBranches.branchOnRetailer(0, 1, 1);
        PricingResult branchedBrute = bruteForcePricing(
            branchedInst, paper, 0, dual, &rightBranches);
        for (int algorithm = 0; algorithm <= 2; ++algorithm) {
            Config pricingConfig = paper;
            pricingConfig.pricing_algorithm = algorithm;
            pricingConfig.rc_eps = 1e-9;
            PricingSolver solver;
            solver.setInstance(branchedInst);
            solver.setConfig(pricingConfig);
            PricingResult actual = solver.solve(0, dual, &rightBranches);
            expectNear(actual.reducedCost, branchedBrute.reducedCost,
                       "right-branch pricing algorithm "
                           + std::to_string(algorithm)
                           + " versus exhaustive RC",
                       1e-7);
            if (actual.optimal_S[0] != 1 || actual.optimal_S[1] != 1
                || actual.optimal_pj > 60.0 + 1e-9
                || !RevenueFormula::serviceSetAcceptsPrice(
                       branchedInst, actual.optimal_S, actual.optimal_pj)) {
                throw std::runtime_error(
                    "right-branch pricing returned an unacceptable price");
            }
        }

        // Multi-column mode promises the best unique positive columns among
        // the per-price optima (not the strict global Top-K over every set).
        // Check that an incumbent for the first column does not stop the scan
        // before lower-ranked positive prices are considered.
        Config multiConfig = paper;
        multiConfig.rc_eps = 1e-9;
        std::vector<PricingResult> expectedMulti = bruteBestColumnPerPrice(
            branchedInst, multiConfig, 0, dual, 3);
        for (int algorithm = 0; algorithm <= 2; ++algorithm) {
            multiConfig.pricing_algorithm = algorithm;
            PricingSolver solver;
            solver.setInstance(branchedInst);
            solver.setConfig(multiConfig);
            std::vector<PricingResult> actual =
                solver.solveMulti(0, dual, 3);
            if (actual.size() != expectedMulti.size()) {
                throw std::runtime_error(
                    "multi-column pricing missed a per-price positive column");
            }
            for (size_t k = 0; k < actual.size(); ++k) {
                expectNear(actual[k].reducedCost, expectedMulti[k].reducedCost,
                           "multi-column candidate " + std::to_string(k),
                           1e-7);
                if (actual[k].optimal_S != expectedMulti[k].optimal_S
                    || std::abs(actual[k].optimal_pj
                                - expectedMulti[k].optimal_pj) > 1e-9) {
                    throw std::runtime_error(
                        "multi-column candidate differs from exhaustive scan");
                }
            }
        }

        std::cout << "PASS: investment cost is included exactly once in every "
                     "paper-model column coefficient.\n";
        std::cout << "PASS: solve statuses map to explicit certified/limited "
                     "outcome categories.\n";
        std::cout << "PASS: GA overall outcomes remain distinct from final "
                     "inner-BP certification.\n";
        std::cout << "PASS: failed GA fitness values cannot corrupt roulette "
                     "selection weights.\n";
        std::cout << "PASS: BP fitness cache keys distinguish adjacent double "
                     "w values.\n";
        std::cout << "PASS: exhaustive service-set check (8/8).\n";
        std::cout << "PASS: post-deduction legacy ranking can select a different "
                     "service set.\n";
        std::cout << "PASS: all pricing algorithms match exhaustive reduced-cost "
                     "enumeration.\n";
        std::cout << "PASS: gamma in {0.25,0.5,0.75,1} is shared by the "
                     "objective and all pricing implementations.\n";
        std::cout << "PASS: right-branch pricing respects the minimum reserve "
                     "price of all forced retailers.\n";
        std::cout << "PASS: multi-column pricing scans all prices that can "
                     "still produce an additional positive column.\n";
        std::cout << "PASS: " << randomizedCases
                  << " deterministic random cases (n=4..12) match exhaustive "
                     "pricing for all three algorithms and branch modes.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
}
