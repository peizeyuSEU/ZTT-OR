#include "../10_common/Config.h"
#include "../10_common/Instance.h"
#include "../7_formula/01_Revenue.h"
#include "../7_formula/02_FacilityCost.h"
#include "../7_formula/03_TransportCost.h"
#include "../7_formula/04_InventoryCost.h"
#include "../7_formula/05_InvestmentCost.h"
#include "../7_formula/08_Objective.h"
#include "../8_solver/01_PricingSolver.h"

#include <cfloat>
#include <cmath>
#include <iostream>
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

double manualBaseProfit(const Instance& inst, const std::vector<int>& S,
                        double price, double w) {
    return RevenueFormula::totalRevenue(inst, 0, S, price)
         - FacilityCostFormula::totalFixedCost(inst, 0, w)
         - TransportCostFormula::totalTransportCost(inst, 0, S, w)
         - InventoryCostFormula::totalInventoryCost(inst, 0, S, w);
}

PricingResult bruteForcePricing(const Instance& inst, const Config& config,
                                int j, const std::vector<double>& dual) {
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

}  // namespace

int main() {
    try {
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

        std::cout << "PASS: investment cost is included exactly once in every "
                     "paper-model column coefficient.\n";
        std::cout << "PASS: exhaustive service-set check (8/8).\n";
        std::cout << "PASS: post-deduction legacy ranking can select a different "
                     "service set.\n";
        std::cout << "PASS: all pricing algorithms match exhaustive reduced-cost "
                     "enumeration.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
}
