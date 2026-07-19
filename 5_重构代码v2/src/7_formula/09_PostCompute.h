#ifndef FORMULA_POST_COMPUTE_H
#define FORMULA_POST_COMPUTE_H

#include "../10_common/Instance.h"
#include "03_TransportCost.h"
#include "04_InventoryCost.h"
#include <vector>
#include <cmath>

/**
 * 09_PostCompute.h — 后处理反算
 *
 * 算法求解过程中消掉了Q_j（EOQ代入）和e⁺/e⁻（碳约束代入），
 * 求解完成后需要将这些变量反算出来用于报告。
 *
 * 反算内容：
 * 1. Q_j*：最优订货量（EOQ公式反算）
 * 2. D_j：每个DC服务的总需求
 * 3. E：总碳排放
 * 4. e⁺ / e⁻：碳交易量
 * 5. 净碳交易成本
 */

class PostComputeFormula {
public:
    /**
     * 1. 反算EOQ最优订货量 Q_j*
     *
     * Q_j* = √(2*(F_j+g_j)*D_j / (h + p̂*ĥ*(1-w_j)))
     *
     * 这是原始EOQ公式（未被消掉前的形式），
     * 与目标函数中消掉Q_j后的√项互为逆运算。
     */
    static double computeQStar(double F_j, double g_j, double D_j,
                                double h, double p_hat,
                                double hat_h, double w_j) {
        if (D_j <= 0) return 0.0;
        double inv_factor = h + p_hat * hat_h * (1.0 - w_j);
        return std::sqrt(2.0 * (F_j + g_j) * D_j / inv_factor);
    }

    /**
     * 2. 计算总碳排放 E
     *
     * 碳排放来源（论文式3.55）：
     *   设施碳排放：   f̂_j * (1-w_j)
     *   运输碳排放：   (b̂_j * D_j + Σ_i b̂_ij * μ_i) * (1-w_j)
     *   库存碳排放：   h_hat * (1-w_j) * (安全库存 + Q_star/2)
     *
     * @param inst       问题实例
     * @param j          DC索引
     * @param S          服务集合
     * @param w_j        减排率
     * @param D_j        总需求
     * @param Q_star     最优订货量（由computeQStar算出）
     * @return 该DC的碳排放
     */
    static double computeCarbonEmission(const Instance& inst, int j,
                                          const std::vector<int>& S,
                                          double w_j, double D_j,
                                          double Q_star) {
        double factor = 1.0 - w_j;

        // 设施碳排放
        double emission = factor * inst.fc[j];

        // 运输碳排放（供应商→DC + DC→零售商）
        double transEmission = inst.b[j] * D_j;
        for (int i = 0; i < inst.numRetailer; i++) {
            if (S[i] == 1) {
                transEmission += inst.bb[i][j] * inst.mu[i];
            }
        }
        emission += factor * transEmission;

        // 库存碳排放（安全库存 + 平均库存）
        double total_var = InventoryCostFormula::totalVariance(inst, j, S);
        double safety_stock = inst.z_alpha * std::sqrt(inst.L[j] * total_var);
        emission += factor * inst.hat_h * safety_stock;
        if (Q_star > 0) {
            emission += factor * inst.hat_h * (Q_star / 2.0);
        }

        return emission;
    }

    /**
     * 3. 计算碳交易量
     *
     * e⁺ = max(E - C, 0)    需要购买的配额
     * e⁻ = max(C - E, 0)    可出售的配额
     */
    static double computeEPositive(double E, double C) {
        return std::max(E - C, 0.0);
    }

    static double computeENegative(double E, double C) {
        return std::max(C - E, 0.0);
    }

    /**
     * 4. 净碳交易成本
     *    如果有盈余（e⁻ > 0），则为负值（收入）
     */
    static double computeNetCarbonCost(double p_hat, double E, double C) {
        double e_plus = computeEPositive(E, C);
        return p_hat * e_plus;
    }
};

#endif // FORMULA_POST_COMPUTE_H
