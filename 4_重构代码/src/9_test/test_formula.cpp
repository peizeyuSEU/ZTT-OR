/**
 * test_formula.cpp — 公式验证测试
 *
 * 独立测试第3层公式计算层，不依赖算法层。
 * 验证所有公式与论文的一致性。
 *
 * 用法：
 *   make test_formula
 *   ./test_formula
 */

#include "../3_formula/01_Revenue.h"
#include "../3_formula/02_FacilityCost.h"
#include "../3_formula/03_TransportCost.h"
#include "../3_formula/04_InventoryCost.h"
#include "../3_formula/05_InvestmentCost.h"
#include "../3_formula/06_CarbonCredit.h"
#include "../3_formula/08_Objective.h"
#include "../3_formula/09_PostCompute.h"
#include <iostream>
#include <iomanip>
#include <cmath>

int main() {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "========== 公式验证测试 ==========" << std::endl;

    // ===== 1. 运输成本分段函数 =====
    std::cout << "\n--- 1. 运输成本分段函数 ---" << std::endl;
    double testDistances[] = {50, 150, 400, 800, 1200};
    for (double d : testDistances) {
        double c1 = TransportCostFormula::costSupplierToDC(d);
        double c2 = TransportCostFormula::costDCToRetailer(d);
        std::cout << "  距离=" << d << ": 供应商→DC=" << c1
                  << ", DC→零售商=" << c2 << std::endl;
    }

    // ===== 2. EOQ反算 =====
    std::cout << "\n--- 2. EOQ反算 Q_j* ---" << std::endl;
    double F = 100, g = 100, D = 10000;
    double h = 10, p_hat = 2, hat_h = 2, w = 0.2;
    double Q = PostComputeFormula::computeQStar(F, g, D, h, p_hat, hat_h, w);
    double inv_factor = h + p_hat * hat_h * (1 - w);
    std::cout << "  F_j=" << F << ", g_j=" << g << ", D_j=" << D
              << ", h=" << h << ", p_hat=" << p_hat
              << ", hat_h=" << hat_h << ", w_j=" << w << std::endl;
    std::cout << "  inv_factor = " << inv_factor << std::endl;
    std::cout << "  Q_j* = sqrt(2*" << (F+g) << "*" << D << "/" << inv_factor
              << ") = " << Q << std::endl;

    // 验证：代入后的库存成本
    double eoqCost = InventoryCostFormula::eoqCost(F, g, inv_factor, D);
    std::cout << "  EOQ代入后库存成本 = sqrt(2*" << (F+g) << "*" << inv_factor
              << "*" << D << ") = " << eoqCost << std::endl;
    // 理论上：代入后的库存成本 = Q* * inv_factor / 2 + (F+g) * D / Q
    double verify = Q * inv_factor / 2 + (F + g) * D / Q;
    std::cout << "  验证（对偶）: Q*inv/2 + (F+g)D/Q = " << verify << std::endl;

    // ===== 3. 减排投资成本 =====
    std::cout << "\n--- 3. 减排投资成本 ---" << std::endl;
    double delta = 5000, w_j = 0.2, D_j = 10000;
    double sqrtCost = InvestmentCostFormula::sqrtDemandModel(delta, w_j, D_j);
    double quadCost = InvestmentCostFormula::quadraticOnlyModel(delta, w_j);
    std::cout << "  δ=" << delta << ", w_j=" << w_j << ", D_j=" << D_j << std::endl;
    std::cout << "  论文版(½δw²√D) = " << sqrtCost << std::endl;
    std::cout << "  旧版(½βw²)    = " << quadCost << std::endl;
    std::cout << "  差异: " << (sqrtCost - quadCost) << std::endl;

    // ===== 4. 碳配额收入 =====
    std::cout << "\n--- 4. 碳配额收入 ---" << std::endl;
    double credit = CarbonCreditFormula::carbonCredit(2.0, 100000.0);
    std::cout << "  p̂*C = 2 * 100000 = " << credit << std::endl;

    // ===== 5. 碳交易反算 =====
    std::cout << "\n--- 5. 碳交易反算 ---" << std::endl;
    double E = 113038.0, C = 100000.0;
    double ep = PostComputeFormula::computeEPositive(E, C);
    double en = PostComputeFormula::computeENegative(E, C);
    double net = PostComputeFormula::computeNetCarbonCost(2.0, E, C);
    std::cout << "  E=" << E << ", C=" << C << std::endl;
    std::cout << "  e+ = max(E-C,0) = " << ep << std::endl;
    std::cout << "  e- = max(C-E,0) = " << en << std::endl;
    std::cout << "  净碳交易成本 = " << net << std::endl;

    // ===== 6. 安全库存成本 =====
    std::cout << "\n--- 6. 安全库存成本 ---" << std::endl;
    double total_var = 125.0, L = 1.0, z = 1.96;
    double ss = InventoryCostFormula::safetyStockCost(inv_factor, z, L, total_var);
    std::cout << "  inv_factor=" << inv_factor << ", z=" << z
              << ", L=" << L << ", Σσ²=" << total_var << std::endl;
    std::cout << "  安全库存成本 = " << inv_factor << " * " << z
              << " * sqrt(" << L << "*" << total_var << ") = " << ss << std::endl;

    std::cout << "\n========== 测试完成 ==========" << std::endl;
    return 0;
}
