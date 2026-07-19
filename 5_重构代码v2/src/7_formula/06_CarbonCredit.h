#ifndef FORMULA_CARBON_CREDIT_H
#define FORMULA_CARBON_CREDIT_H

/**
 * 06_CarbonCredit.h — 碳配额收入（常数项）
 *
 * 对应论文式3.62中的 p̂ * C 项。
 *
 * 在目标函数重写过程中，通过碳约束 e⁺ - e⁻ = E - C 代入消掉了 e⁺ 和 e⁻，
 * 得到常数项 p̂ * C。
 *
 * 实际碳交易量 e⁺ / e⁻ 在后处理中反算（见 09_PostCompute.h）。
 */

class CarbonCreditFormula {
public:
    /**
     * 碳配额收入（常数项）
     * @param p_hat  碳配额交易价格 p̂
     * @param C      初始碳配额
     * @return p̂ * C
     */
    static double carbonCredit(double p_hat, double C) {
        return p_hat * C;
    }
};

#endif // FORMULA_CARBON_CREDIT_H
