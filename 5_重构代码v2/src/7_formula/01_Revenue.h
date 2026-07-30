#ifndef FORMULA_REVENUE_H
#define FORMULA_REVENUE_H

#include "../10_common/Instance.h"

/**
 * 01_Revenue.h — 收入计算
 *
 * 对应论文式3.52/式4.2：
 * 零售商i愿意支付的单价取决于批发价 p_j 和保留价 v_i 的关系
 *
 * r_i(p_j) = p_j    (p_j ≤ v_i)
 *          = 0      (p_j > v_i)
 *
 * 总收入 = Σ_i r_i(p_j) * μ_i * Y_ij
 *
 * 注意：当 p_j > v_i 时零售商不接受服务，
 * 实际使用中应在调用前通过 S 集合过滤已服务的零售商。
 */

class RevenueFormula {
public:
    /**
     * 检查服务集合与统一批发价是否相容。
     * 模型含义是 S_i=1 表示零售商实际接受服务，因此必须有 p_j<=v_i。
     */
    static bool serviceSetAcceptsPrice(const Instance& inst,
                                       const std::vector<int>& S,
                                       double p_j,
                                       double eps = 1e-9) {
        for (int i = 0; i < inst.numRetailer && i < (int)S.size(); i++) {
            if (S[i] == 1 && p_j > inst.reservePrice[i] + eps) return false;
        }
        return true;
    }

    /**
     * 计算零售商i的单价收入
     * @param p_j    配送中心制定的批发价
     * @param v_i    零售商的保留价
     * @return 单位收入 r_i(p_j)
     */
    static double unitRevenue(double p_j, double v_i) {
        return (p_j <= v_i) ? p_j : 0.0;
    }

    /**
     * 计算DC j服务集合S的总收入
     * @param inst   问题实例
     * @param j      DC索引
     * @param S      服务集合（二进制向量）
     * @param p_j    批发价
     * @return 总收入
     */
    static double totalRevenue(const Instance& inst, int j,
                                const std::vector<int>& S, double p_j) {
        double revenue = 0.0;
        for (int i = 0; i < inst.numRetailer; i++) {
            if (S[i] == 1) {
                revenue += unitRevenue(p_j, inst.reservePrice[i]) * inst.mu[i];
            }
        }
        return revenue;
    }
};

#endif // FORMULA_REVENUE_H
