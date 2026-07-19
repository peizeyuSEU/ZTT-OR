#ifndef RESULT_WRITER_H
#define RESULT_WRITER_H

#include "../0_base/01_types/Instance.h"
#include "../0_base/01_types/Solution.h"
#include "../0_base/03_utils/Logger.h"
#include "01_PostProcessor.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>

/**
 * 结果报告生成器
 *
 * 对应论文第10.4节：生成完整的决策方案报告
 * 包含：问题规模、最优解汇总、网络决策、DC详情、碳交易信息
 */
class ResultWriter {
public:
    /**
     * 写入完整决策方案报告
     */
    static void writeReport(const std::string& filename,
                            const Instance& inst,
                            const Solution& sol,
                            const std::vector<double>& bestW,
                            double baseProfit,
                            double solveTime,
                            const PostProcessor& postProc) {

        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[ResultWriter] 无法写入 " << filename << std::endl;
            return;
        }

        file << std::fixed << std::setprecision(2);

        file << "--- 完整决策方案报告 ---\n\n";
        file << "生成时间: " << Logger::getTimestamp() << "\n\n";

        // 问题规模
        file << "问题规模:\n";
        file << "  DC数量: " << inst.numDC << "\n";
        file << "  零售商数量: " << inst.numRetailer << "\n\n";

        // 最优解汇总
        file << "最优解汇总:\n";
        file << "  总利润: " << sol.totalProfit << "\n";
        file << "  总碳排放: " << postProc.getCarbonResult().E << "\n";
        file << "  求解时间: " << solveTime << " 秒\n\n";

        // 网络决策
        file << "网络决策:\n";
        file << "  开设DC数量: " << sol.numDCsOpen << "\n";
        file << "  服务零售商数量: " << sol.numRtsServed << "\n";
        file << "  基准方案利润（w=0）: " << baseProfit << "\n\n";

        // DC详情
        file << "DC详情:\n";
        for (const auto& dc : postProc.dcResults) {
            file << "  DC " << dc.index << ":\n";
            file << "    是否开设: " << (dc.isOpen ? "是" : "否") << "\n";
            if (dc.isOpen) {
                file << "    减排率 w_" << dc.index << ": " << dc.w << "\n";
                file << "    批发价 p_" << dc.index << ": " << dc.p << "\n";
                file << "    服务总需求 D_" << dc.index << ": " << dc.D << "\n";
                file << "    最优订货量 Q_" << dc.index << "*: " << dc.Q_star << "\n";
                file << "    服务的零售商: [";
                for (size_t i = 0; i < dc.servedRetailers.size(); i++) {
                    if (i > 0) file << ", ";
                    file << dc.servedRetailers[i];
                }
                file << "]\n";
            }
            file << "\n";
        }

        // 碳交易
        const auto& carbon = postProc.getCarbonResult();
        file << "碳交易:\n";
        file << "  初始碳配额 C: " << inst.C << "\n";
        file << "  实际碳排放 E: " << carbon.E << "\n";
        file << "  需要购买 e+: " << carbon.e_plus << "\n";
        file << "  可出售 e-: " << carbon.e_minus << "\n";
        file << "  净碳交易成本: " << carbon.netCost << "\n";

        file.close();

        // 同时输出到控制台
        std::cout << "\n--- 完整决策方案报告 ---\n";
        std::cout << "问题规模: #DC=" << inst.numDC << ", #零售商=" << inst.numRetailer << "\n";
        std::cout << "总利润: " << sol.totalProfit << "\n";
        std::cout << "总碳排放: " << carbon.E << "\n";
        std::cout << "求解时间: " << solveTime << "s\n";
        std::cout << "开设DC数量: " << sol.numDCsOpen << "\n";
        std::cout << "服务零售商数量: " << sol.numRtsServed << "\n\n";

        for (const auto& dc : postProc.dcResults) {
            std::cout << "DC " << dc.index << ": "
                      << (dc.isOpen ? "开设" : "不开设");
            if (dc.isOpen) {
                std::cout << ", w=" << dc.w
                          << ", p=" << dc.p
                          << ", D=" << dc.D
                          << ", Q*=" << dc.Q_star;
            }
            std::cout << "\n";
        }

        std::cout << "\n碳排放 E=" << carbon.E
                  << ", e+=" << carbon.e_plus
                  << ", e-=" << carbon.e_minus
                  << ", 净碳交易成本=" << carbon.netCost << "\n";
    }
};

#endif // RESULT_WRITER_H
