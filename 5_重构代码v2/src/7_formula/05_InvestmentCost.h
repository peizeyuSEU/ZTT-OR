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
 *   投资成本 = ½ * δ * w_j² * D_j^γ, 0 < γ <= 1
 *
 * 其中 θ_j(D_j) = δ * D_j^γ 为减排投资成本系数，
 * 体现规模经济效应：需求越大，减排难度越大，但边际增长率递减。
 *
 * 历史版本 ½ * β_j * w_j² 可通过公式开关保留用于回归。
 */

class InvestmentCostFormula {
public:
    /**
     * 一般化论文模型：½ * δ * w_j² * D_j^γ
     *
     * @param delta  减排成本系数
     * @param w_j    减排率
     * @param D_j    总需求
     */
    static double demandScaleModel(double delta, double w_j, double D_j,
                                   double exponent = 0.5) {
        if (D_j <= 0.0) return 0.0;
        return 0.5 * delta * w_j * w_j * std::pow(D_j, exponent);
    }

    // Backward-compatible name for the manuscript baseline gamma=0.5.
    static double sqrtDemandModel(double delta, double w_j, double D_j) {
        return demandScaleModel(delta, w_j, D_j, 0.5);
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
     * @param useDemandScale true=需求规模模型，false=历史 1/2 beta_j w_j^2
     * @param exponent   需求规模指数 gamma，须满足 0 < gamma <= 1
     */
    static double compute(const Instance& inst, int j,
                           double w_j, double D_j,
                           bool useDemandScale = true,
                           double exponent = 0.5) {
        if (useDemandScale) {
            return demandScaleModel(inst.delta, w_j, D_j, exponent);
        } else {
            return quadraticOnlyModel(inst.beta[j], w_j);
        }
    }
};

#endif // FORMULA_INVESTMENT_COST_H
