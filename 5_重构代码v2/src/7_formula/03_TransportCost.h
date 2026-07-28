#ifndef FORMULA_TRANSPORT_COST_H
#define FORMULA_TRANSPORT_COST_H

#include "../10_common/Instance.h"
#include <vector>
#include <cmath>

/**
 * 03_TransportCost.h — 运输成本 + 运输碳排放成本
 *
 * 对应论文式5-1（供应商→DC）和式5-2（DC→零售商）的分段函数
 * 以及式4.2中的运输碳排放成本项。
 *
 * 运输成本采用分段函数，体现规模经济效应：
 *   供应商→DC：c(a_j) 分段，同一距离内比DC→零售商便宜（整车 vs 零担）
 *   DC→零售商：c(d_ij) 分段
 *
 * 运输碳排放成本 = p̂ * b̂ * (1-w_j) * D
 *   其中 b̂_j = k * a_j（供应商→DC）
 *        b̂_ij = k * d_ij（DC→零售商）
 */

class TransportCostFormula {
public:
    /**
     * 是否使用距离直接作为运输成本（原版代码方式，无分段函数）
     */
    static bool useDirectDistance;

    /**
     * 供应商到DC的运输成本分段函数（论文式5-1）
     */
    static double costSupplierToDC(double distance) {
        if (useDirectDistance) return distance;
        if (distance <= 100.0) return 5.0;
        if (distance <= 300.0) return 10.0;
        if (distance <= 600.0) return 15.0;
        if (distance <= 1000.0) return 20.0;
        return 25.0;
    }

    /**
     * DC到零售商的运输成本分段函数（论文式5-2）
     */
    static double costDCToRetailer(double distance) {
        if (useDirectDistance) return distance;
        if (distance <= 100.0) return 10.0;
        if (distance <= 300.0) return 20.0;
        if (distance <= 600.0) return 30.0;
        if (distance <= 1000.0) return 40.0;
        return 50.0;
    }

    /**
     * 计算DC j服务集合S的运输成本（不含碳）
     * 供应商→DC成本 + DC→零售商成本
     *
     * = c(a_j) * D_j + Σ_i c(d_ij) * μ_i * Y_ij
     */
    static double transportCost(const Instance& inst, int j,
                                 const std::vector<int>& S) {
        double costSupToDC = costSupplierToDC(inst.a_dist[j]);
        double total = 0.0;

        for (int i = 0; i < inst.numRetailer; i++) {
            if (S[i] == 1) {
                double costDCToRet = costDCToRetailer(inst.dist[i][j]);
                total += (costSupToDC + costDCToRet) * inst.mu[i];
            }
        }
        return total;
    }

    /**
     * 计算DC j服务集合S的运输碳排放成本
     * = p̂ * (1-w_j) * (b̂_j * D_j + Σ_i b̂_ij * μ_i)
     *
     * 其中 b̂_j = k * a_j, b̂_ij = k * d_ij
     */
    static double transportCarbonCost(const Instance& inst, int j,
                                       const std::vector<int>& S,
                                       double w_j) {
        double p_hat = inst.p;
        double factor = p_hat * (1.0 - w_j);
        double total = 0.0;

        // 供应商→DC碳排放
        double carbonSupToDC = inst.b[j];  // b̂_j
        // DC→零售商碳排放
        for (int i = 0; i < inst.numRetailer; i++) {
            if (S[i] == 1) {
                total += (carbonSupToDC + inst.bb[i][j]) * inst.mu[i];
            }
        }
        return factor * total;
    }

    /**
     * 计算DC j服务集合S的总运输相关成本（运输成本+碳排放成本）
     */
    static double totalTransportCost(const Instance& inst, int j,
                                      const std::vector<int>& S,
                                      double w_j) {
        return transportCost(inst, j, S)
             + transportCarbonCost(inst, j, S, w_j);
    }
};

// 静态成员定义
bool TransportCostFormula::useDirectDistance = false;

#endif // FORMULA_TRANSPORT_COST_H
