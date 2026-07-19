/**
 * test_pricing.cpp — 定价子问题算法对比测试
 *
 * 对随机生成的问题实例，用简化凸包算法和论文 Algorithm 3
 * 分别求解定价子问题，对比两者结果是否一致。
 *
 * 用法：
 *   make test-pricing
 *   ../bin/test_pricing
 */

#include "../2_config/Config.h"
#include "../0_base/01_types/Instance.h"
#include "../0_base/02_data/DataGenerator.h"
#include "../4_algorithm/01_PricingSolver.h"
#include "../0_base/03_utils/Logger.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <algorithm>

int main() {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "========== 定价子问题算法对比测试 ==========" << std::endl;

    Config config;
    config.num_dc = 3;
    config.num_retailers = 10;
    config.random_seed = 32501;
    config.delta = 5000.0;

    DataGenerator gen(config.random_seed);
    Instance inst = gen.generate(config);
    inst.w.resize(inst.numDC, 0.2);

    std::cout << "实例: #DC=" << inst.numDC << ", #Rts=" << inst.numRetailer << std::endl;

    PricingSolver solver;
    solver.setInstance(inst);
    solver.setConfig(config);
    solver.setRCEps(1e-6);

    int totalTests = 0;
    int passedTests = 0;
    double maxDiff = 0.0;

    // ===== 对每个 DC × 每个候选价格 进行对比 =====
    for (int j = 0; j < inst.numDC; j++) {
        std::vector<double> dual(inst.numRetailer + inst.numDC);
        for (int i = 0; i < (int)dual.size(); i++) {
            dual[i] = (double)((i * 137 + j * 251) % 100000) / 100.0 - 500.0;
        }

        std::vector<double> prices;
        for (int i = 0; i < inst.numRetailer; i++) {
            prices.push_back(inst.reservePrice[i]);
        }
        std::sort(prices.begin(), prices.end());
        prices.erase(std::unique(prices.begin(), prices.end()), prices.end());

        std::cout << "\n--- DC " << j << " ---" << std::endl;

        for (double p_j : prices) {
            double rc1, rc2;
            bool ok = solver.verifyBothAlgorithms(j, p_j, dual, rc1, rc2);
            double diff = std::abs(rc1 - rc2);
            maxDiff = std::max(maxDiff, diff);
            totalTests++;

            std::string status;
            if (ok && diff < 1e-4) {
                status = "✅";
                passedTests++;
            } else {
                status = "❌";
            }

            std::cout << "  " << status << " p_j=" << std::setw(8) << p_j
                      << "  RC简化=" << std::setw(14) << rc1
                      << "  RC算法3=" << std::setw(14) << rc2
                      << "  差=" << diff << std::endl;

            if (!ok) {
                // 额外调试信息：看看 num 情况
                // 通过设置开关再调用 solve() 来间接获取信息
            }
        }
    }

    std::cout << "\n========== 结果汇总 ==========" << std::endl;
    std::cout << "总测试数: " << totalTests << std::endl;
    std::cout << "通过: " << passedTests << "/" << totalTests;
    if (passedTests == totalTests) {
        std::cout << " ✅ 全部一致" << std::endl;
    } else {
        std::cout << " ❌ 有差异" << std::endl;
    }
    std::cout << "最大差值: " << maxDiff << std::endl;

    // solve() 整体对比
    std::cout << "\n--- 完整 solve() 对比（最优价格选择）---" << std::endl;
    for (int j = 0; j < inst.numDC; j++) {
        std::vector<double> dual(inst.numRetailer + inst.numDC);
        for (int i = 0; i < (int)dual.size(); i++) {
            dual[i] = (double)((i * 137 + j * 251) % 100000) / 100.0 - 500.0;
        }

        config.use_paper_algorithm3 = false;
        solver.setConfig(config);
        PricingResult r1 = solver.solve(j, dual);

        config.use_paper_algorithm3 = true;
        solver.setConfig(config);
        PricingResult r2 = solver.solve(j, dual);

        double diff = std::abs(r1.reducedCost - r2.reducedCost);
        std::string status = (diff < 1e-4) ? "✅" : "❌";
        std::cout << "  " << status << " DC " << j
                  << ": 简化RC=" << r1.reducedCost
                  << " (p=" << r1.optimal_pj << ")"
                  << "  算法3RC=" << r2.reducedCost
                  << " (p=" << r2.optimal_pj << ")"
                  << "  差=" << diff << std::endl;
    }

    // ==== 额外诊断：对失败的案例，暴力枚举所有子集验证 ====
    std::cout << "\n--- 对失败案例暴力枚举验证 ---" << std::endl;
    for (int j = 0; j < inst.numDC; j++) {
        std::vector<double> dual(inst.numRetailer + inst.numDC);
        for (int i = 0; i < (int)dual.size(); i++) {
            dual[i] = (double)((i * 137 + j * 251) % 100000) / 100.0 - 500.0;
        }

        std::vector<double> prices;
        for (int i = 0; i < inst.numRetailer; i++)
            prices.push_back(inst.reservePrice[i]);
        std::sort(prices.begin(), prices.end());
        prices.erase(std::unique(prices.begin(), prices.end()), prices.end());

        for (double p_j : prices) {
            double rc1, rc2;
            solver.verifyBothAlgorithms(j, p_j, dual, rc1, rc2);
            if (std::abs(rc1 - rc2) < 1e-4) continue;

            // 暴力枚举所有 2^n 子集
            double bruteBestRC = -1e100;
            std::vector<int> bruteBestS;
            int n = inst.numRetailer;
            for (int mask = 0; mask < (1 << n); mask++) {
                std::vector<int> S(n, 0);
                for (int i = 0; i < n; i++) {
                    if (mask & (1 << i)) S[i] = 1;
                }
                // 用简化版算法相同的 computeReducedCost（需要访问私有方法）
                // 这里我们用 solve 的 simplified 版本来验证
                // 但为了简单，只输出结论
            }

            std::cout << "DC " << j << " p_j=" << p_j
                      << " 简化RC=" << rc1 << " 算法3RC=" << rc2
                      << " — 需要暴力枚举验证哪个是真正的全局最优" << std::endl;
        }
    }

    std::cout << "\n注意: 若有差异，说明 Algorithm 3 或简化算法中有一个"
              << "未能找到全局最优解。需要暴力枚举来验证哪个是正确的。" << std::endl;

    return (passedTests == totalTests) ? 0 : 1;
}
