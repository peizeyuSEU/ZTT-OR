#ifndef COLUMN_GENERATION_H
#define COLUMN_GENERATION_H

#include "../0_base/01_types/Instance.h"
#include "../0_base/01_types/Types.h"
#include "../2_config/Config.h"
#include "../3_formula/08_Objective.h"
#include "../0_base/04_solvers/CplexWrapper.h"
#include "../0_base/03_utils/Logger.h"
#include "../0_base/03_utils/ThreadPool.h"
#include "01_PricingSolver.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <chrono>
#include <memory>

/**
 * 列生成求解器（Column Generation）
 *
 * 采用与原代码 GA_BP.cpp 一致的方式：
 * 在同一个 CPLEX 环境中动态添加新列，无需每轮重建模型。
 */
class ColumnGeneration {
private:
    const Instance* inst;
    const Config* config = nullptr;
    PricingSolver pricingSolver;
    ObjectiveFormula objFormula;
    Logger* logger;
    double rc_eps;
    double cgTime;
    int totalIterations;
    int totalColumns;

public:
    ColumnGeneration() : inst(nullptr), logger(nullptr), rc_eps(1e-6), cgTime(0.0),
                         totalIterations(0), totalColumns(0) {}

    void setInstance(const Instance& instance) {
        inst = &instance;
        pricingSolver.setInstance(instance);
    }

    void setConfig(const Config& cfg) {
        config = &cfg;
        pricingSolver.setConfig(cfg);
    }

    void setRCEps(double eps) {
        rc_eps = eps;
        pricingSolver.setRCEps(eps);
    }

    void setLogger(Logger* log) {
        logger = log;
        pricingSolver.setLogger(log);
    }

    double getCGTime() const { return cgTime; }
    int getIterations() const { return totalIterations; }
    int getTotalColumns() const { return totalColumns; }

    CGResult solve(const BranchState& branchState) {
        auto start = std::chrono::steady_clock::now();
        CGResult result;
        result.feasible = false;

        CplexEnv env;

        try {
            int numDC = inst->numDC;
            int numRetailer = inst->numRetailer;

            // ===== 1. 初始化列集合 =====
            std::vector<std::vector<std::vector<int>>> j_S(numDC);
            std::vector<std::vector<double>> p_j(numDC);
            std::vector<std::vector<double>> profits(numDC);

            for (int j = 0; j < numDC; j++) {
                // 空集列
                std::vector<int> emptyS(numRetailer, 0);
                double emptyProfit = objFormula.columnProfit(*inst, j, emptyS, 0.0, 0.0);
                j_S[j].push_back(emptyS);
                p_j[j].push_back(0.0);
                profits[j].push_back(emptyProfit);

                // 每个零售商单独服务的列
                for (int i = 0; i < numRetailer; i++) {
                    std::vector<int> S(numRetailer, 0);
                    S[i] = 1;
                    double price = inst->reservePrice[i];
                    double profit = objFormula.columnProfit(*inst, j, S, price, 0.0);
                    j_S[j].push_back(S);
                    p_j[j].push_back(price);
                    profits[j].push_back(profit);
                }
            }

            // ===== 2. 构建RMP模型（一次性构建，后续动态加列）=====
            IloEnv envRaw = env.get();
            IloModel model(envRaw);
            IloObjective totalProfit = IloAdd(model, IloMaximize(envRaw));

            int numConstraints = numRetailer + numDC;
            IloNumArray up(envRaw, numConstraints);
            for (int k = 0; k < numConstraints; k++) {
                up[k] = 1;
            }
            IloRangeArray Fill = IloAdd(model, IloRangeArray(envRaw, -IloInfinity, up));

            IloArray<IloNumVarArray> z_j(envRaw);

            for (int j = 0; j < numDC; j++) {
                IloNumVarArray z_j_s(env.get());
                for (int m = 0; m < (int)profits[j].size(); m++) {
                    IloNumColumn col = totalProfit(profits[j][m]);
                    for (int n = 0; n < numRetailer; n++) {
                        if (j_S[j][m][n] == 1) {
                            col = col + Fill[n](1);
                        }
                    }
                    int kk = numRetailer + j;
                    col = col + Fill[kk](1);
                    z_j_s.add(IloNumVar(col, 0, 1, ILOFLOAT));
                }
                z_j.add(z_j_s);
            }

            // 分支约束
            for (int i = 0; i < branchState.addSize; i++) {
                int dcIdx = branchState.branchId[i * 2];
                int colIdx = branchState.branchId[i * 2 + 1];
                if (dcIdx < z_j.getSize() && colIdx < z_j[dcIdx].getSize()) {
                    z_j[dcIdx][colIdx].setBounds(branchState.lb[i], branchState.ub[i]);
                }
            }

            // ===== 3. 创建CPLEX求解器（只创建一次）=====
            IloCplex cplex(model);
            cplex.setOut(envRaw.getNullStream());
            cplex.setParam(IloCplex::Threads, 1);
            cplex.setParam(IloCplex::EpOpt, 1e-9);

            if (logger) {
                logger->log(2, "列生成开始，RMP初始列数=" + std::to_string(numDC * (numRetailer + 1)));
            }

            // ===== 4. 列生成迭代 =====
            IloNumArray price(envRaw, numConstraints);

            for (int iter = 0; ; iter++) {
                totalIterations++;

                if (logger) logger->log(2, "RMP迭代 #" + std::to_string(iter + 1));

                // 4a. 求解RMP
                if (!cplex.solve()) {
                    if (logger) logger->log(2, "RMP求解失败（不可行）");
                    result.feasible = false;
                    auto end = std::chrono::steady_clock::now();
                    cgTime += std::chrono::duration<double>(end - start).count();
                    return result;
                }

                double objValue = cplex.getObjValue();

                if (logger) {
                    logger->log(2, "RMP迭代 #" + std::to_string(iter + 1)
                                + " 完成，目标值 = " + std::to_string(objValue));
                }

                // 4b. 检查整数性
                bool isInteger = true;
                for (int j = 0; j < z_j.getSize() && isInteger; j++) {
                    for (int k = 0; k < z_j[j].getSize() && isInteger; k++) {
                        double val = cplex.getValue(z_j[j][k]);
                        if (!CplexEnv::isIntegerValue(val, rc_eps)) {
                            isInteger = false;
                        }
                    }
                }

                result.lpObjective = objValue;
                result.isInteger = isInteger;
                result.feasible = true;
                result.final_z_j.resize(z_j.getSize());
                for (int j = 0; j < z_j.getSize(); j++) {
                    result.final_z_j[j].resize(z_j[j].getSize());
                    for (int k = 0; k < z_j[j].getSize(); k++) {
                        result.final_z_j[j][k] = cplex.getValue(z_j[j][k]);
                    }
                }
                result.j_S = j_S;
                result.p_j = p_j;
                result.numColumns = totalColumns;

                // 4c. 获取对偶变量（即使整数解也需要，用于检查定价子问题是否还有正检验数列）
                for (int i = 0; i < numConstraints; i++) {
                    price[i] = cplex.getDual(Fill[i]);
                }

                // 4d. 调用定价子问题
                if (logger) logger->log(2, "调用定价子问题");

                std::vector<double> dualVec(numConstraints);
                for (int i = 0; i < numConstraints; i++) {
                    dualVec[i] = price[i];
                }

                // 4e. 收集所有DC中RC>0的列
                std::vector<PricingResult> positiveCols;

                if (numDC > 1 && config && config->parallel_pricing) {
                    // 并行求解各DC的定价子问题（每个线程用独立的PricingSolver）
                    int pThreads = std::min(numDC, 4);
                    ThreadPool pPool(pThreads);
                    std::vector<std::future<PricingResult>> pFutures(numDC);
                    for (int j = 0; j < numDC; j++) {
                        pFutures[j] = pPool.enqueue([this, j, &dualVec]() {
                            PricingSolver localSolver;
                            localSolver.setInstance(*inst);
                            if (config) localSolver.setConfig(*config);
                            localSolver.setRCEps(rc_eps);
                            return localSolver.solve(j, dualVec);
                        });
                    }
                    for (int j = 0; j < numDC; j++) {
                        PricingResult pr = pFutures[j].get();
                        if (logger) {
                            std::string s = "j=" + std::to_string(j)
                                + ", p=" + std::to_string(pr.optimal_pj)
                                + ", S={";
                            for (int i = 0; i < inst->numRetailer; i++) {
                                if (pr.optimal_S[i] == 1) s += std::to_string(i) + ",";
                            }
                            if (s.back() == ',') s.pop_back();
                            s += "}, 检验数=" + std::to_string(pr.reducedCost);
                            logger->log(4, s);
                        }
                        if (pr.reducedCost > rc_eps) {
                            positiveCols.push_back(pr);
                        }
                    }
                } else {
                    for (int j = 0; j < numDC; j++) {
                        PricingResult pr = pricingSolver.solve(j, dualVec);
                        if (logger) {
                            std::string s = "j=" + std::to_string(j)
                                + ", p=" + std::to_string(pr.optimal_pj)
                                + ", S={";
                            for (int i = 0; i < inst->numRetailer; i++) {
                                if (pr.optimal_S[i] == 1) s += std::to_string(i) + ",";
                            }
                            if (s.back() == ',') s.pop_back();
                            s += "}, 检验数=" + std::to_string(pr.reducedCost);
                            logger->log(4, s);
                        }
                        if (pr.reducedCost > rc_eps) {
                            positiveCols.push_back(pr);
                        }
                    }
                }

                // 4f. 收敛判断
                if (positiveCols.empty()) {
                    if (logger) logger->log(2, "所有检验数 <= 0，列生成收敛");
                    auto end = std::chrono::steady_clock::now();
                    cgTime += std::chrono::duration<double>(end - start).count();
                    return result;
                }

                // 4g. 一次性加入所有RC>0的列（加速收敛）
                if (logger) logger->log(2, "本轮加入 " + std::to_string(positiveCols.size()) + " 列");
                for (const auto& colResult : positiveCols) {
                    int optJ = colResult.dcIndex;
                    const std::vector<int>& optS = colResult.optimal_S;
                    double optPj = colResult.optimal_pj;
                    double newProfit = objFormula.columnProfit(*inst, optJ, optS, optPj, inst->w[optJ]);

                    IloNumColumn col = totalProfit(newProfit);
                    for (int n = 0; n < numRetailer; n++) {
                        if (optS[n] == 1) {
                            col = col + Fill[n](1);
                        }
                    }
                    int jIdx = numRetailer + optJ;
                    col = col + Fill[jIdx](1);
                    z_j[optJ].add(IloNumVar(col, 0, 1, ILOFLOAT));

                    j_S[optJ].push_back(optS);
                    p_j[optJ].push_back(optPj);
                    profits[optJ].push_back(newProfit);
                    totalColumns++;
                }
            }

        } catch (const IloException& ex) {
            std::cerr << "[ColumnGeneration] CPLEX异常: " << ex.getMessage() << std::endl;
            result.feasible = false;
        } catch (const std::exception& ex) {
            std::cerr << "[ColumnGeneration] 异常: " << ex.what() << std::endl;
            result.feasible = false;
        }

        auto end = std::chrono::steady_clock::now();
        cgTime += std::chrono::duration<double>(end - start).count();
        return result;
    }
};

#endif // COLUMN_GENERATION_H
