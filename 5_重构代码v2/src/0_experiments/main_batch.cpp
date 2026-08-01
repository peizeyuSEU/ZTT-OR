/**
 * 批量实验启动器
 *
 * 用法：
 *   ./bin/batch            # 默认跑 table4_1
 *   ./bin/batch delta_scan # 跑 delta_scan 方案
 *
 * 实验方案文件放在 src/0_experiments/configs/ 下
 */

#include "ExperimentPlan.h"
#include "../2_orchestrator/Orchestrator.h"
#include "../10_common/OutputManager.h"
#include "../5_result/ResultWriter.h"
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
    std::string planName = "table4_1";
    if (argc >= 2) planName = argv[1];

    std::string planPath = "src/0_experiments/configs/" + planName + ".yaml";

    // 1. 加载实验方案
    ExperimentPlan plan;
    if (!plan.load(planPath)) {
        std::cerr << "[实验层] 无法加载实验方案: " << planPath << std::endl;
        return 1;
    }

    // 2. 创建实验输出目录
    const std::string outputPlanName =
        plan.planName().empty() ? planName : plan.planName();
    std::string expDir = OutputManager::createExperimentDir(outputPlanName);
    std::cout << "实验结果目录: " << expDir << std::endl;

    // 3. 保存本次实验的方案配置
    plan.saveConfigTo(expDir + "/config_used.yaml");

    // 4. 展开所有实验条目
    std::vector<Config> configs = plan.expand();

    // 5. 依次运行每个条目
    std::vector<ExperimentResult> allResults;
    for (size_t i = 0; i < configs.size(); i++) {
        Config& cfg = configs[i];
        std::string entryName = plan.entryName(i);

        // 为每个条目创建独立子目录
        std::string entryDir = OutputManager::createEntryDir(expDir, entryName);

        // 设置输出路径
        cfg.output_dir = entryDir;
        cfg.log_file = entryDir + "/run.log";

        std::cout << "\n运行: " << entryName
                  << " (#DC=" << cfg.num_dc
                  << ", #Rts=" << cfg.num_retailers
                  << ", δ=" << cfg.delta
                  << ", seed=" << cfg.random_seed << ")" << std::endl;

        // 把展开后的 Config 写为条目内的运行输入（也是可复现的实验存档）。
        // 必须只通过 Config 的统一序列化入口写出，避免这里手写字段清单后漏掉
        // rc_eps、transport_direct_distance 等会影响结果的配置。
        std::string tmpConfig = entryDir + "/config_used.yaml";
        std::string configError;
        if (!cfg.validate(&configError)) {
            std::cerr << "[实验层] 条目 " << entryName
                      << " 的配置无效: " << configError << std::endl;
            return 1;
        }
        if (!cfg.saveToFile(tmpConfig)) {
            std::cerr << "[实验层] 无法写入条目配置: " << tmpConfig << std::endl;
            return 1;
        }

        Orchestrator orchestrator2;
        orchestrator2.initialize(tmpConfig, entryDir);
        orchestrator2.run();
        orchestrator2.finalize();

        // 用实验方案里的条目名覆盖结果名称，避免汇总表「名称」列显示默认 exp
        ExperimentResult res = orchestrator2.getResult();
        res.experimentName = entryName;
        allResults.push_back(res);
    }

    // 6. 输出汇总对比表格
    ResultWriter::saveExperimentTable(expDir + "/summary.txt", allResults);
    ResultWriter::saveExperimentCSV(expDir + "/summary.csv", allResults);

    std::cout << "\n========== 批量实验完成 ==========" << std::endl;
    std::cout << "结果已保存至: " << expDir << std::endl;
    return 0;
}
