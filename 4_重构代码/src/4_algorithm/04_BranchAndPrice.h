#ifndef BRANCH_AND_PRICE_H
#define BRANCH_AND_PRICE_H

#include "../0_base/01_types/Instance.h"
#include "../0_base/01_types/Types.h"
#include "../0_base/01_types/Solution.h"
#include "../2_config/Config.h"
#include "../0_base/03_utils/Logger.h"
#include "../3_formula/06_CarbonCredit.h"
#include "../3_formula/05_InvestmentCost.h"
#include "../3_formula/04_InventoryCost.h"
#include "02_ColumnGeneration.h"
#include "03_BranchAndBound.h"
#include <vector>
#include <iostream>
#include <chrono>
#include <cmath>

/**
 * 分支定价封装层（Branch and Price）
 *
 * 对应论文第3层：列生成 + 分支定界 = 分支定价
 * 在固定 w_j 下精确求解整数规划问题。
 *
 * 日志层级：Level 1 [BP]，缩进2空格
 */
class BranchAndPrice {
private:
    const Instance* inst;
    const Config* config;
    Logger* logger;
    ColumnGeneration columnGen;
    BranchAndBound bnb;
    double bpTime;

    // 缓存：对相同的w向量直接返回已计算的适应度
    struct CacheEntry {
        double profit;
        Solution sol;
    };
    std::unordered_map<std::string, CacheEntry> fitnessCache;

public:
    BranchAndPrice() : inst(nullptr), config(nullptr), logger(nullptr), bpTime(0.0) {}

    void setInstance(const Instance& instance) {
        inst = &instance;
        columnGen.setInstance(instance);
        bnb.setInstance(instance);
    }

    void setConfig(const Config& cfg) {
        config = &cfg;
        columnGen.setConfig(cfg);
        columnGen.setRCEps(cfg.rc_eps);
        bnb.setConfig(cfg);
    }

    void setLogger(Logger* log) {
        logger = log;
        columnGen.setLogger(log);
        bnb.setLogger(log);
    }

    double getBPTime() const { return bpTime; }

    /**
     * 对固定w求解分支定价
     *
     * @param w 减排率向量
     * @param label 个体标识（用于日志）
     * @return 最大利润（适应度）
     */
    double solve(const std::vector<double>& w, const std::string& label = "") {
        auto start = std::chrono::steady_clock::now();

        // 检查缓存
        std::string key = makeKey(w);
        auto cacheIt = fitnessCache.find(key);
        if (cacheIt != fitnessCache.end()) {
            if (logger) logger->log(1, label + "命中缓存，w=[" + wToString(w) + "]");
            return cacheIt->second.profit;
        }

        if (logger) {
            std::string indent = "  ";
            logger->log(1, "分支定价开始" + (label.empty() ? "" : " " + label)
                        + "，w=[" + wToString(w) + "]");
        }

        // 设置减排率
        Instance& mutableInst = const_cast<Instance&>(*inst);
        mutableInst.setEmissionReductionRates(w);

        // 调用分支定界求解（w已设置，无需传参）
        double result = bnb.solve();

        // 加回碳配额收入
        if (result > -DBL_MAX / 2) {
            result += CarbonCreditFormula::carbonCredit(inst->p, inst->C);
            // 统一扣除减排投资成本（在列利润中不包含，在此统一处理）
            // 使用公式层计算：½ * δ * w_j² * √D_j（论文标准版）
            double investCost = 0.0;
            for (int j = 0; j < inst->numDC; j++) {
                double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
                // 计算D_j需要知道该DC服务的零售商标的，
                // 但投资成本是每个DC的固定成本，不依赖具体服务集合S，
                // 这里用分支定界最优解中该DC的D_j来算
                double D_j = 0.0;
                const Solution& sol = bnb.getBestSolution();
                for (const auto& dcSol : sol.dcSolutions) {
                    if (dcSol.dcIndex == j) {
                        for (int i = 0; i < inst->numRetailer; i++) {
                            if (dcSol.S[i] == 1) D_j += inst->mu[i];
                        }
                        break;
                    }
                }
                bool useSqrtModel = config ? config->use_sqrt_investment : true;
                investCost += InvestmentCostFormula::compute(*inst, j, w_j, D_j, useSqrtModel);
            }
            result -= investCost;
        }

        // 存入缓存
        CacheEntry entry;
        entry.profit = result;
        entry.sol = bnb.getBestSolution();
        fitnessCache[key] = entry;

        auto end = std::chrono::steady_clock::now();
        bpTime += std::chrono::duration<double>(end - start).count();

        if (logger) {
            logger->log(1, label + "适应度 = " + std::to_string(result));
        }

        return result;
    }

    /** 获取最优解 */
    const Solution& getBestSolution() const { return bnb.getBestSolution(); }

    /** 获取分支定界统计 */
    int getTotalNodes() const { return bnb.getTotalNodes(); }
    int getPrunedNodes() const { return bnb.getPrunedNodes(); }
    int getIntegerSolutions() const { return bnb.getIntegerSolutions(); }

private:
    static std::string makeKey(const std::vector<double>& w) {
        std::string key;
        for (double v : w) {
            key += std::to_string(static_cast<int>(v * 10000)) + ",";
        }
        return key;
    }

    static std::string wToString(const std::vector<double>& w) {
        std::string s;
        for (size_t i = 0; i < w.size(); i++) {
            if (i > 0) s += ", ";
            s += std::to_string(w[i]);
        }
        return s;
    }
};

#endif // BRANCH_AND_PRICE_H
