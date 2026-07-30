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

        // 把展开后的 Config 写为条目内的临时 yaml，交给 Orchestrator 加载。
        // 说明：Orchestrator.initialize() 仅接受 configPath，因此这里落盘一份
        // 完整字段的 config_used.yaml 作为运行输入（同时也是可复现的实验存档）。
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
            f << "min_w: " << cfg.min_w << "\n";
            f << "max_w: " << cfg.max_w << "\n";
            f << "delta: " << cfg.delta << "\n";
            // 碳交易参数
            f << "carbon_price: " << cfg.carbon_price << "\n";
            f << "carbon_cap: " << cfg.carbon_cap << "\n";
            f << "inv_carbon_coeff: " << cfg.inv_carbon_coeff << "\n";
            f << "transport_carbon_coeff: " << cfg.transport_carbon_coeff << "\n";
            f << "fac_carbon_ratio: " << cfg.fac_carbon_ratio << "\n";
            // 服务水平与成本参数
            f << "service_level: " << cfg.service_level << "\n";
            f << "z_alpha: " << cfg.z_alpha << "\n";
            f << "order_fixed_cost: " << cfg.order_fixed_cost << "\n";
            f << "transport_fixed_cost: " << cfg.transport_fixed_cost << "\n";
            f << "lead_time: " << cfg.lead_time << "\n";
            f << "holding_cost: " << cfg.holding_cost << "\n";
            // 数据生成范围（保证与论文实例一致）
            f << "random_seed: " << cfg.random_seed << "\n";
            f << "coord_min: " << cfg.coord_min << "\n";
            f << "coord_max: " << cfg.coord_max << "\n";
            f << "mu_min: " << cfg.mu_min << "\n";
            f << "mu_max: " << cfg.mu_max << "\n";
            f << "var_min: " << cfg.var_min << "\n";
            f << "var_max: " << cfg.var_max << "\n";
            f << "fixed_cost_min: " << cfg.fixed_cost_min << "\n";
            f << "fixed_cost_max: " << cfg.fixed_cost_max << "\n";
            f << "reserve_price_min: " << cfg.reserve_price_min << "\n";
            f << "reserve_price_max: " << cfg.reserve_price_max << "\n";
            f << "supplier_x: " << cfg.supplier_x << "\n";
            f << "supplier_y: " << cfg.supplier_y << "\n";
            // 公式模型与定价算法（核心：确保用选定的方案0）
            f << "use_sqrt_investment: " << (cfg.use_sqrt_investment ? "true" : "false") << "\n";
            f << "use_invest_in_column: " << (cfg.use_invest_in_column ? "true" : "false") << "\n";
            f << "pricing_algorithm: " << cfg.pricing_algorithm << "\n";
            f << "pricing_max_cols_per_dc: " << cfg.pricing_max_cols_per_dc << "\n";
            f << "pricing_adaptive_cols: " << (cfg.pricing_adaptive_cols ? "true" : "false") << "\n";
            f << "pricing_adaptive_stall_iterations: " << cfg.pricing_adaptive_stall_iterations << "\n";
            f << "pricing_adaptive_max_cols: " << cfg.pricing_adaptive_max_cols << "\n";
            f << "pricing_per_dc_by_w: " << (cfg.pricing_per_dc_by_w ? "true" : "false") << "\n";
            f << "pricing_high_w_threshold: " << cfg.pricing_high_w_threshold << "\n";
            f << "pricing_high_w_max_cols: " << cfg.pricing_high_w_max_cols << "\n";
            // 早停与并行参数（保证批量与单跑性能一致）
            f << "early_stop: " << (cfg.early_stop ? "true" : "false") << "\n";
            f << "convergence_generations: " << cfg.convergence_generations << "\n";
            f << "convergence_tolerance: " << cfg.convergence_tolerance << "\n";
            f << "max_cg_iterations: " << cfg.max_cg_iterations << "\n";
            f << "max_branch_nodes: " << cfg.max_branch_nodes << "\n";
            f << "bb_node_strategy: " << cfg.bb_node_strategy << "\n";
            f << "bb_hybrid_switch_gap: " << cfg.bb_hybrid_switch_gap << "\n";
            f << "bb_hybrid_dfs_quota: " << cfg.bb_hybrid_dfs_quota << "\n";
            f << "bb_adaptive_stagnation_nodes: " << cfg.bb_adaptive_stagnation_nodes << "\n";
            f << "bb_adaptive_cooldown_nodes: " << cfg.bb_adaptive_cooldown_nodes << "\n";
            f << "bp_time_limit_sec: " << cfg.bp_time_limit_sec << "\n";
            f << "bp_relative_gap: " << cfg.bp_relative_gap << "\n";
            f << "root_heuristic: " << (cfg.root_heuristic ? "true" : "false") << "\n";
            f << "root_rmp_mip_heuristic: "
              << (cfg.root_rmp_mip_heuristic ? "true" : "false") << "\n";
            f << "root_rmp_mip_time_limit_sec: "
              << cfg.root_rmp_mip_time_limit_sec << "\n";
            f << "cg_early_stop: " << (cfg.cg_early_stop ? "true" : "false") << "\n";
            f << "cg_stall_iterations: " << cfg.cg_stall_iterations << "\n";
            f << "cg_stall_tolerance: " << cfg.cg_stall_tolerance << "\n";
            f << "parallel_fitness: " << (cfg.parallel_fitness ? "true" : "false") << "\n";
            f << "num_threads: " << cfg.num_threads << "\n";
            f << "parallel_pricing: " << (cfg.parallel_pricing ? "true" : "false") << "\n";
            f << "pricing_threads: " << cfg.pricing_threads << "\n";
            f << "core_budget: " << cfg.core_budget << "\n";
            f << "cplex_threads: " << cfg.cplex_threads << "\n";
            f << "run_mode: " << cfg.run_mode << "\n";
            // 对偶平滑系数（0=关闭）。必须落盘，否则 Orchestrator 重新加载时
            // 会回退到 Config 默认值 0.5，导致 smooth_off 对照组实际未关闭平滑。
            f << "dual_smooth_alpha: " << cfg.dual_smooth_alpha << "\n";
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

    std::cout << "\n========== 批量实验完成 ==========" << std::endl;
    std::cout << "结果已保存至: " << expDir << std::endl;
    return 0;
}
