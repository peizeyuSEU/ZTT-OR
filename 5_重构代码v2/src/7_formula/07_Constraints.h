#ifndef FORMULA_CONSTRAINTS_H
#define FORMULA_CONSTRAINTS_H

#include "../10_common/Instance.h"
#include <vector>

/**
 * 07_Constraints.h — 约束条件
 *
 * 对应论文式3.55-3.62中的约束：
 *
 * 1. 碳约束（式3.55）：
 *    E * (1-w_j) + (e⁻ - e⁺) = C
 *    代入消掉 e⁺, e⁻ 后，碳约束不再显式出现在目标函数中，
 *    碳排放通过 p̂ * (1-w_j) * E 项在目标函数中货币化。
 *
 * 2. 唯一分配约束（式3.56）：
 *    Σ_j Y_ij ≤ 1, ∀i
 *    每个零售商最多由一个DC服务。
 *
 * 3. DC开设约束（式3.57）：
 *    Y_ij ≤ X_j, ∀i,j
 *    零售商只能分配给已开设的DC。
 *
 * 4. 变量范围：
 *    Q_j ≥ 0, p_j ≥ 0, 0 ≤ w_j ≤ 1
 *    Y_ij ∈ {0,1}, X_j ∈ {0,1}
 *    e⁺ ≥ 0, e⁻ ≥ 0
 *
 * 注意：约束检查主要在算法层（列生成的RMP中通过CPLEX施加），
 * 这里仅提供约束正确性的验证函数。
 */

class ConstraintsFormula {
public:
    /**
     * 检查唯一分配约束
     * 每个零售商最多被一个DC服务
     *
     * @param S_by_DC  每个DC的服务集合（二进制向量）
     * @param numRetailer  零售商数量
     * @return true 如果所有零售商最多被分配一次
     */
    static bool checkUniqueAssignment(
        const std::vector<std::vector<int>>& S_by_DC,
        int numRetailer) {

        for (int i = 0; i < numRetailer; i++) {
            int count = 0;
            for (const auto& S : S_by_DC) {
                if (i < (int)S.size() && S[i] == 1) {
                    count++;
                }
            }
            if (count > 1) return false;
        }
        return true;
    }

    /**
     * 检查DC开设约束
     * 零售商只能分配给已开设的DC
     *
     * @param S_by_DC  每个DC的服务集合
     * @param X        各DC是否开设
     * @return true 如果合法
     */
    static bool checkDCOpenConstraint(
        const std::vector<std::vector<int>>& S_by_DC,
        const std::vector<int>& X) {

        for (size_t j = 0; j < S_by_DC.size(); j++) {
            if (X[j] == 0) {
                for (int i = 0; i < (int)S_by_DC[j].size(); i++) {
                    if (S_by_DC[j][i] == 1) return false;
                }
            }
        }
        return true;
    }
};

#endif // FORMULA_CONSTRAINTS_H
