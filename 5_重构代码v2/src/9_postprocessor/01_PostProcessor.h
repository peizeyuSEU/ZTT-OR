#ifndef POST_PROCESSOR_H
#define POST_PROCESSOR_H

#include "../10_common/Instance.h"
#include "../10_common/Types.h"
#include "../10_common/Solution.h"
#include "../7_formula/09_PostCompute.h"
#include "../7_formula/04_InventoryCost.h"
#include <vector>
#include <cmath>
#include <iostream>

/**
 * 后处理模块
 *
 * 对应论文第10节：在算法求解完成后，计算被EOQ公式消掉的决策变量。
 *
 * 计算内容：
 * 1. Q_j*：每个已开设DC的最优订货量（EOQ公式）
 * 2. D_j：每个DC服务的总需求
 * 3. E：总碳排放
 * 4. e+ / e-：碳交易量（需要购买/可出售的配额）
 * 5. 净碳交易成本
 */
class PostProcessor {
public:
    // ===== 输出数据 =====
    struct DCResult {
        int index;
        bool isOpen;
        double w;                   // 减排率
        double p;                   // 批发价
        double D;                   // 服务总需求
        double Q_star;              // 最优订货量
        std::vector<int> servedRetailers;  // 服务的零售商索引
        double profit;              // 该DC利润
    };

    struct CarbonResult {
        double E;                   // 实际碳排放（不含碳交易）
        double e_plus;              // 需要购买的配额 max(E-C, 0)
        double e_minus;             // 可出售的配额 max(C-E, 0)
        double netCost;             // 净碳交易成本
    };

    std::vector<DCResult> dcResults;
    CarbonResult carbonResult;

    PostProcessor(const Instance& instance, const Solution& solution,
                  const std::vector<double>& bestW)
        : inst(instance), sol(solution), w(bestW) {}

    /** 执行后处理计算 */
    void compute() {
        dcResults.clear();

        for (int j = 0; j < inst.numDC; j++) {
            DCResult dc;
            dc.index = j;
            dc.w = (j < (int)w.size()) ? w[j] : 0.0;

            // 查找该DC是否在最优解中被开设
            dc.isOpen = false;
            dc.D = 0.0;
            dc.p = 0.0;
            dc.profit = 0.0;

            for (const auto& dcSol : sol.dcSolutions) {
                if (dcSol.dcIndex == j) {
                    dc.isOpen = true;
                    dc.p = dcSol.p;
                    dc.profit = dcSol.profit;

                    // 收集服务的零售商
                    for (int i = 0; i < inst.numRetailer; i++) {
                        if (dcSol.S[i] == 1) {
                            dc.servedRetailers.push_back(i);
                            dc.D += inst.mu[i];
                        }
                    }
                    break;
                }
            }

            // 计算Q_j*（EOQ公式，论文第10.1节）→ 调 formula/09_PostCompute
            if (dc.isOpen && dc.D > 0) {
                dc.Q_star = PostComputeFormula::computeQStar(
                    inst.F[j], inst.g[j], dc.D,
                    inst.h, inst.p, inst.hat_h, dc.w);
            } else {
                dc.Q_star = 0.0;
            }

            dcResults.push_back(dc);
        }

        // 计算碳排放和碳交易量
        computeCarbon();
    }

    /** 获取碳排放结果 */
    const CarbonResult& getCarbonResult() const { return carbonResult; }

    /** 打印后处理结果 */
    void print() const {
        std::cout << "\n--- 后处理结果 ---" << std::endl;
        for (const auto& dc : dcResults) {
            std::cout << "DC " << dc.index << ": "
                      << (dc.isOpen ? "开设" : "不开设")
                      << ", w=" << dc.w
                      << ", p=" << dc.p
                      << ", D=" << dc.D
                      << ", Q*=" << dc.Q_star
                      << ", 服务零售商={";
            for (size_t i = 0; i < dc.servedRetailers.size(); i++) {
                if (i > 0) std::cout << ",";
                std::cout << dc.servedRetailers[i];
            }
            std::cout << "}" << std::endl;
        }

        std::cout << "碳排放 E=" << carbonResult.E
                  << ", e+=" << carbonResult.e_plus
                  << ", e-=" << carbonResult.e_minus
                  << ", 净碳交易成本=" << carbonResult.netCost << std::endl;
    }

private:
    const Instance& inst;
    const Solution& sol;
    const std::vector<double>& w;

    /** 计算碳排放和碳交易量（论文第10.2节）→ 调 formula/09_PostCompute */
    void computeCarbon() {
        double E = 0.0;

        for (const auto& dc : dcResults) {
            if (!dc.isOpen) continue;

            int idx = dc.index;
            // 重建 S 向量（从 servedRetailers 转回二进制向量）
            std::vector<int> S(inst.numRetailer, 0);
            for (int r : dc.servedRetailers) {
                S[r] = 1;
            }

            E += PostComputeFormula::computeCarbonEmission(
                inst, idx, S, dc.w, dc.D, dc.Q_star);
        }

        carbonResult.E = E;
        carbonResult.e_plus = PostComputeFormula::computeEPositive(E, inst.C);
        carbonResult.e_minus = PostComputeFormula::computeENegative(E, inst.C);
        carbonResult.netCost = PostComputeFormula::computeNetCarbonCost(inst.p, E, inst.C);
    }
};

#endif // POST_PROCESSOR_H
