#ifndef FORMULA_FACILITY_COST_H
#define FORMULA_FACILITY_COST_H

#include "../0_base/01_types/Instance.h"

/**
 * 02_FacilityCost.h — DC固定成本 + DC固定碳排放成本
 *
 * 对应论文式4.2中的两项：
 *   DC固定成本：         f_j * X_j
 *   DC固定碳排放成本：   p̂ * f̂_j * (1-w_j) * X_j
 *
 * 碳排放成本反映在目标函数中，通过碳交易价格 p̂ 将碳排放量货币化。
 */

class FacilityCostFormula {
public:
    /**
     * 计算DC j的固定成本（不含碳排放）
     */
    static double fixedCost(double f_j) {
        return f_j;
    }

    /**
     * 计算DC j的固定碳成本
     * @param p_hat   碳配额交易价格 p̂
     * @param fc_j    设施碳排放 f̂_j
     * @param w_j     减排率
     * @return 固定碳成本
     */
    static double fixedCarbonCost(double p_hat, double fc_j, double w_j) {
        return p_hat * fc_j * (1.0 - w_j);
    }

    /**
     * 计算DC j的总固定成本（含碳）
     * = f_j + p̂ * f̂_j * (1-w_j)
     */
    static double totalFixedCost(const Instance& inst, int j, double w_j) {
        return inst.f[j] + inst.p * inst.fc[j] * (1.0 - w_j);
    }
};

#endif // FORMULA_FACILITY_COST_H
