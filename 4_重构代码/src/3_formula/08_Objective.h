#ifndef FORMULA_OBJECTIVE_H
#define FORMULA_OBJECTIVE_H

#include "../0_base/01_types/Instance.h"
#include "../2_config/Config.h"
#include "01_Revenue.h"
#include "02_FacilityCost.h"
#include "03_TransportCost.h"
#include "04_InventoryCost.h"
#include "05_InvestmentCost.h"
#include "06_CarbonCredit.h"
#include <vector>
#include <cmath>

/**
 * 08_Objective.h — 总目标函数
 *
 * 汇总 01~06 各项，计算DC j服务集合S在定价p_j、减排率w_j下的净收益。
 *
 * 对应论文式4-2中的 R̄_{j,S}：
 *
 * R̄_{j,S} = 收入
 *          - DC固定成本（含碳）
 *          - 运输成本（含碳）
 *          - 库存成本（EOQ+安全库存，含碳）
 *          - 减排投资成本
 *          + 碳配额收入（常数项，加在总分上）
 *
 * 注意：
 * - 减排投资成本不包含在列利润中（避免LP松弛中分数解重复扣除），
 *   而是在 BranchAndPrice::solve() 中统一扣除。
 * - 此处同时提供包含和不包含投资成本的两个版本。
 */

class ObjectiveFormula {
private:
    const Config* config;

public:
    ObjectiveFormula() : config(nullptr) {}
    ObjectiveFormula(const Config& cfg) : config(&cfg) {}

    void setConfig(const Config& cfg) { config = &cfg; }

    /**
     * 计算DC j的列利润 R̄_{j,S}（不含减排投资成本）
     *
     * 此版本用于列生成中的RMP，因为投资成本不在列中扣除。
     *
     * R̄_{j,S} = 收入 - 固定成本(含碳) - 运输成本(含碳) - 库存成本(含碳)
     */
    double columnProfit(const Instance& inst, int j,
                         const std::vector<int>& S, double p_j,
                         double w_j) const {

        // 01 收入
        double revenue = RevenueFormula::totalRevenue(inst, j, S, p_j);

        // 02 DC固定成本（含碳）
        double facilityCost = FacilityCostFormula::totalFixedCost(inst, j, w_j);

        // 03 运输成本（含碳）
        double transportCost = TransportCostFormula::totalTransportCost(
            inst, j, S, w_j);

        // 04 库存成本（EOQ+安全库存，含碳）
        double inventoryCost = InventoryCostFormula::totalInventoryCost(
            inst, j, S, w_j);

        return revenue - facilityCost - transportCost - inventoryCost;
    }

    /**
     * 计算DC j的总净收益（含减排投资成本）
     *
     * 此版本用于BranchAndPrice最终汇总。
     *
     * @param useSqrtModel  是否使用√D_j版本的减排投资成本模型
     */
    double totalProfit(const Instance& inst, int j,
                        const std::vector<int>& S, double p_j,
                        double w_j, bool useSqrtModel = true) const {

        double profit = columnProfit(inst, j, S, p_j, w_j);

        // 05 减排投资成本
        double D_j = InventoryCostFormula::totalDemand(inst, j, S);
        double investCost = InvestmentCostFormula::compute(
            inst, j, w_j, D_j, useSqrtModel);

        profit -= investCost;

        return profit;
    }

    /**
     * 计算全局总利润（含碳配额收入常数项）
     *
     * @param sumColumnProfits  各DC列利润之和（不含投资成本）
     * @param sumInvestCosts    各DC投资成本之和
     * @param inst              问题实例
     */
    double globalTotalProfit(double sumColumnProfits,
                              double sumInvestCosts,
                              const Instance& inst) const {

        // 06 碳配额收入
        double credit = CarbonCreditFormula::carbonCredit(inst.p, inst.C);

        return sumColumnProfits - sumInvestCosts + credit;
    }

    /**
     * 计算R̄_{j,S}（论文式4-2，含投资成本，用于报告）
     */
    double getRjs(const Instance& inst, int j,
                   const std::vector<int>& S, double p_j,
                   double w_j) const {
        return totalProfit(inst, j, S, p_j, w_j);
    }
};

#endif // FORMULA_OBJECTIVE_H
