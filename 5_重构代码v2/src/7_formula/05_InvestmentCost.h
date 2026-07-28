#ifndef FORMULA_INVESTMENT_COST_H
#define FORMULA_INVESTMENT_COST_H

#include "../10_common/Instance.h"
#include <string>
#include <cmath>

/**
 * 05_InvestmentCost.h — 减排投资成本
 *
 * 对应论文式4.2中的减排投资成本项：
 *
 *   投资成本 = ½ * δ * w_j² * √D_j
 *
 * 其中 θ_j(D_j) = δ * √D_j 为减排投资成本系数，
 * 体现规模经济效应：需求越大，减排难度越大，但边际增长率递减。
 *
 * 注意：当前代码使用 ½ * β_j * w_j²（无√D_j），
 * 与论文不一致，通过公式开关切换。
 */

class InvestmentCostFormula {
public:
    /**
     * 论文标准版：½ * δ * w_j² * √D_j
     *
     * @param delta  减排成本系数
     * @param w_j    减排率
     * @param D_j    总需求
     */
    static double sqrtDemandModel(double delta, double w_j, double D_j) {
        return 0.5 * delta * w_j * w_j * std::sqrt(D_j);
    }

    /**
     * 旧版（当前代码）：½ * β_j * w_j²
     *
     * @param beta_j  减排成本系数（各DC不同）
     * @param w_j     减排率
     */
    static double quadraticOnlyModel(double beta_j, double w_j) {
        return 0.5 * beta_j * w_j * w_j;
    }

    /**
     * 根据配置选择模型计算
     *
     * @param inst       问题实例
     * @param j          DC索引
     * @param w_j        减排率
     * @param D_j        总需求
     * @param useSqrt    是否使用√D_j版本（true=论文标准，false=旧版）
     */
    static double compute(const Instance& inst, int j,
                           double w_j, double D_j, bool useSqrt = true) {
        if (useSqrt) {
            return sqrtDemandModel(inst.delta, w_j, D_j);
        } else {
            return quadraticOnlyModel(inst.beta[j], w_j);
        }
    }
};

#endif // FORMULA_INVESTMENT_COST_H
