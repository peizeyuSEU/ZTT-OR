#ifndef FORMULA_INVENTORY_COST_H
#define FORMULA_INVENTORY_COST_H

#include "../10_common/Instance.h"
#include <vector>
#include <cmath>

/**
 * 04_InventoryCost.h — 库存成本（EOQ代入后）+ 安全库存成本
 *
 * 对应论文式4.2中EOQ代入后的两项：
 *
 * 1. 订货+持有成本（EOQ代入后）：
 *    √(2*(F_j+g_j)*(h + p̂*ĥ*(1-w_j))*D_j)
 *
 * 2. 安全库存成本：
 *    (h + p̂*ĥ*(1-w_j)) * z_α * √(L_j * Σ_i σ²_i * Y_ij)
 *
 * EOQ推导：原始模型中有Q_j（订货量变量），
 * 对Q_j求导=0得 Q_j* = √(2(F_j+g_j)D_j / (h + p̂*ĥ*(1-w_j)))
 * 代回目标函数后得到上述第一个公式，Q_j被消掉。
 *
 * 后处理时通过 09_PostCompute.h 反算 Q_j*。
 */

class InventoryCostFormula {
public:
    /**
     * 计算库存持有成本因子
     * inv_factor = h + p̂ * ĥ * (1-w_j)
     *
     * 这个因子同时出现在EOQ项和安全库存项中。
     */
    static double inventoryCostFactor(double h, double p_hat,
                                       double hat_h, double w_j) {
        return h + p_hat * hat_h * (1.0 - w_j);
    }

    /**
     * 计算总需求 D_j = Σ_i μ_i * Y_ij
     */
    static double totalDemand(const Instance& inst, int j,
                               const std::vector<int>& S) {
        double D = 0.0;
        for (int i = 0; i < inst.numRetailer; i++) {
            if (S[i] == 1) {
                D += inst.mu[i];
            }
        }
        return D;
    }

    /**
     * 计算总方差 Σ_i σ²_i * Y_ij
     */
    static double totalVariance(const Instance& inst, int j,
                                 const std::vector<int>& S) {
        double var = 0.0;
        for (int i = 0; i < inst.numRetailer; i++) {
            if (S[i] == 1) {
                var += inst.variance[i];
            }
        }
        return var;
    }

    /**
     * 1. EOQ代入后的订货+持有成本
     *
     * √(2*(F_j+g_j)*(h + p̂*ĥ*(1-w_j))*D_j)
     *
     * @param F_j    订货固定成本
     * @param g_j    运输固定成本
     * @param inv_factor  库存持有成本因子（含碳）
     * @param D_j    总需求
     */
    static double eoqCost(double F_j, double g_j,
                           double inv_factor, double D_j) {
        return std::sqrt(2.0 * (F_j + g_j) * inv_factor * D_j);
    }

    /**
     * 2. 安全库存成本
     *
     * inv_factor * z_α * √(L_j * Σσ²_i)
     *
     * @param inv_factor  库存持有成本因子（含碳）
     * @param z_alpha    标准正态分位数
     * @param L_j        补货提前期
     * @param total_var  总方差 Σσ²_i
     */
    static double safetyStockCost(double inv_factor, double z_alpha,
                                   double L_j, double total_var) {
        return inv_factor * z_alpha * std::sqrt(L_j * total_var);
    }

    /**
     * 计算DC j服务集合S的总库存成本（EOQ+安全库存）
     */
    static double totalInventoryCost(const Instance& inst, int j,
                                      const std::vector<int>& S,
                                      double w_j) {
        double D_j = totalDemand(inst, j, S);
        if (D_j <= 0) return 0.0;

        double inv_factor = inventoryCostFactor(
            inst.h, inst.p, inst.hat_h, w_j);
        double total_var = totalVariance(inst, j, S);

        double cost = eoqCost(inst.F[j], inst.g[j], inv_factor, D_j)
                    + safetyStockCost(inv_factor, inst.z_alpha,
                                      inst.L[j], total_var);
        return cost;
    }
};

#endif // FORMULA_INVENTORY_COST_H
