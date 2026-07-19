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
    std::string expDir = OutputManager::createExperimentDir(plan.planName());
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

        // 运行
        Orchestrator orchestrator;
        orchestrator.initialize(entryDir + "/../config_used.yaml");
        // 注意：Orchestrator 的 initialize 会读取配置文件
        // 我们直接用扩展后的 Config，需要改造 Orchestrator 支持直接传 Config
        // 当前方案：先把 Config 写为临时文件，让 Orchestrator 读取
        // 更好的方案：Orchestrator 支持直接传 Config

        // 由于当前 Orchestrator.initialize() 接受 configPath，
        // 我们直接传临时文件路径不优雅。简单处理：
        // 把 cfg 保存为临时 yaml，让 Orchestrator 加载
        std::string tmpConfig = entryDir + "/config_used.yaml";
        {
            std::ofstream f(tmpConfig);
            f << "num_dc: " << cfg.num_dc << "\n";
            f << "num_retailers: " << cfg.num_retailers << "\n";
            f << "population_size: " << cfg.population_size << "\n";
            f << "max_generation: " << cfg.max_generation << "\n";
            f << "crossover_rate: " << cfg.crossover_rate << "\n";
            f << "mutation_rate: " << cfg.mutation_rate << "\n";
            f << "elitism: " << (cfg.elitism ? "true" : "false") << "\n";
            f << "chromosome_length: " << cfg.chromosome_length << "\n";
            f << "delta: " << cfg.delta << "\n";
            f << "carbon_price: " << cfg.carbon_price << "\n";
            f << "carbon_cap: " << cfg.carbon_cap << "\n";
            f << "random_seed: " << cfg.random_seed << "\n";
            f << "holding_cost: " << cfg.holding_cost << "\n";
            f << "use_sqrt_investment: " << (cfg.use_sqrt_investment ? "true" : "false") << "\n";
            f << "run_mode: " << cfg.run_mode << "\n";
            if (!cfg.fixed_w.empty()) {
                f << "fixed_w: [";
                for (size_t k = 0; k < cfg.fixed_w.size(); k++) {
                    if (k > 0) f << ", ";
                    f << cfg.fixed_w[k];
                }
                f << "]\n";
            }
            f << "legacy_mode: " << (cfg.legacy_mode ? "true" : "false") << "\n";
            f.close();
        }

        Orchestrator orchestrator2;
        orchestrator2.initialize(tmpConfig);
        orchestrator2.run();
        orchestrator2.finalize();

        allResults.push_back(orchestrator2.getResult());
    }

    // 6. 输出汇总对比表格
    ResultWriter::saveExperimentTable(expDir + "/summary.txt", allResults);

    std::cout << "\n========== 批量实验完成 ==========" << std::endl;
    std::cout << "结果已保存至: " << expDir << std::endl;
    return 0;
}
