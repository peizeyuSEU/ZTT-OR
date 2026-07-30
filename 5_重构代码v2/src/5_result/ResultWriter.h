#ifndef RESULT_WRITER_H
#define RESULT_WRITER_H

#include "ExperimentResult.h"
#include "../10_common/Instance.h"
#include "../9_postprocessor/01_PostProcessor.h"
#include "../3_monitor/Monitor.h"
#include "../10_common/Config.h"
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <cmath>

/**
 * 实验结果输出模块
 *
 * 收集运行结果并输出到文件：
 * - saveReport：单次运行的完整决策方案报告
 * - saveConvergenceCSV：收敛曲线 CSV
 * - saveExperimentTable：批量实验对比表格
 */
class ResultWriter {
public:
    void collectFrom(const Monitor& monitor,
                     const std::vector<double>& w,
                     double profit, double emission,
                     double solveTime, int dcOpen, int rtServed,
                     int branchNodes, int gaGenerations = 0,
                     double carbonCap = 0.0) {
        result_.totalProfit = profit;
        result_.carbonEmission = emission;
        result_.carbonCap = carbonCap;
        result_.ePlus = emission > carbonCap ? emission - carbonCap : 0.0;
        result_.eMinus = carbonCap > emission ? carbonCap - emission : 0.0;
        result_.netCarbonTradingCost =
            result_.carbonPrice * (emission - carbonCap);
        result_.solveTime = solveTime;
        result_.numDCsOpen = dcOpen;
        result_.numRtsServed = rtServed;
        result_.bestW = w;
        result_.totalBranchNodes = branchNodes;
        result_.prunedNodes = monitor.prunedNodes();
        result_.totalCGIterations = monitor.columnGenIterations();
        result_.totalColumnsGenerated = monitor.totalColumns();
        result_.totalGAGenerations = gaGenerations > 0 ? gaGenerations : monitor.currentGeneration() + 1;
        result_.convergence = monitor.convergenceHistory();
        // 收集完整时间分布
        result_.timing = monitor.buildTimingReport();
        // 收集完整报告文本（与终端/run.log 统一格式）
        result_.timingReportText = monitor.formatTimingReport();
        result_.summaryText = monitor.formatSummary();
    }

    void setMeta(const std::string& name, int numDC, int numRt,
                  double delta, int seed) {
        result_.experimentName = name;
        result_.numDC = numDC;
        result_.numRetailers = numRt;
        result_.delta = delta;
        result_.randomSeed = seed;
    }

    void setConfigMeta(const Config& config) {
        result_.investmentExponent = config.investment_exponent;
        result_.configuredMutationRate = config.mutation_rate;
        const int totalBits =
            std::max(1, config.num_dc * config.chromosome_length);
        result_.effectiveMutationRate =
            config.mutation_rate > 0.0
                ? config.mutation_rate
                : 1.0 / static_cast<double>(totalBits);
        result_.carbonPrice = config.carbon_price;
        result_.pricingAlgorithm = config.pricing_algorithm;
        result_.pricingMaxColsPerDC = config.pricing_max_cols_per_dc;
        result_.runMode = config.run_mode;
    }

    void setSolution(const std::vector<DCSolution>& dcSol) {
        result_.dcSolutions = dcSol;
    }

    void setSolveCertification(const Solution& sol) {
        result_.solveStatus = sol.solveStatus;
        result_.bestBound = sol.bestBound;
        result_.relativeGap = sol.relativeGap;
        result_.hasIntegerSolution = sol.hasIntegerSolution;
        result_.totalInvestmentCost = sol.totalInvestmentCost;
    }

    void setTotalInvestmentCost(double cost) {
        result_.totalInvestmentCost = cost;
    }

    const ExperimentResult& getResult() const { return result_; }

    /** 保存完整决策方案报告 */
    void saveReport(const std::string& path, const Instance& inst,
                    const std::vector<double>& w,
                    double baseProfit, const Config& config) {
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[ResultWriter] 无法写入 " << path << std::endl;
            return;
        }

        file << "--- 完整决策方案报告 ---\n\n";
        file << "问题规模:\n";
        file << "  DC数量: " << inst.numDC << "\n";
        file << "  零售商数量: " << inst.numRetailer << "\n\n";

        // ===== 运行配置 =====
        file << "运行配置:\n";
        // 并行 / 串行模式
        bool is_parallel = config.parallel_fitness && config.num_threads != 1;
        file << "  适应度评估模式: "
             << (is_parallel ? "并行 (fork 多进程)" : "串行") << "\n";
        if (is_parallel) {
            file << "  并行线程/进程数: " << config.num_threads
                 << (config.num_threads == 0 ? " (0=自动检测CPU核心数)" : "") << "\n";
        }
        file << "  定价子问题并行: "
             << (config.parallel_pricing ? "是" : "否") << "\n";
        file << "  随机种子: " << config.random_seed << "\n";
        // 遗传算法参数
        file << "  [遗传算法]\n";
        file << "    种群规模: " << config.population_size << "\n";
        file << "    最大迭代代数: " << config.max_generation << "\n";
        file << "    交叉率: " << config.crossover_rate << "\n";
        double effectiveMutationRate =
            config.mutation_rate > 0.0
                ? config.mutation_rate
                : 1.0 / static_cast<double>(
                      config.num_dc * config.chromosome_length);
        file << "    逐位变异率: " << effectiveMutationRate
             << (config.mutation_rate > 0.0 ? " (固定)\n"
                                            : " (自适应 1/L)\n");
        file << "    染色体长度: " << config.chromosome_length << "\n";
        file << "    GA早停: " << (config.early_stop ? "启用" : "关闭");
        if (config.early_stop) {
            file << " (连续 " << config.convergence_generations
                 << " 代相对改善 < " << config.convergence_tolerance << " 则停)";
        }
        file << "\n";
        // 列生成参数
        file << "  [列生成]\n";
        file << "    BP节点列生成: 严格认证（tailing-off不用于BB节点退出）\n";
        file << "    最大迭代次数: " << config.max_cg_iterations << "\n";
        file << "    reduced cost 阈值: " << config.rc_eps << "\n";
        file << "    tailing-off 早停: "
             << (config.cg_early_stop ? "启用" : "关闭");
        if (config.cg_early_stop) {
            file << " (连续 " << config.cg_stall_iterations
                 << " 轮相对改善 < " << config.cg_stall_tolerance << " 则停)";
        }
        file << "\n";
        // 模型开关
        file << "  [模型]\n";
        file << "    减排系数 delta: " << config.delta << "\n";
        file << "    投资函数: "
             << (config.use_sqrt_investment
                     ? "1/2·delta·w^2·D^" + std::to_string(
                           config.investment_exponent)
                                            : "1/2·beta·w^2 (历史模型)")
             << "\n";
        const char* algo_name =
            (config.pricing_algorithm == 1)
                ? "论文 Algorithm3 (完整凸包+余弦相似度)"
                : (config.pricing_algorithm == 2)
                      ? "ConvexHull/Simplified 候选 + Algorithm3 完整校验"
                      : "Simplified 候选 + Algorithm3 完整校验";
        file << "    定价算法: " << algo_name << "\n\n";

        file << "求解认证:\n";
        file << "  状态: " << solveStatusName(result_.solveStatus) << "\n";
        file << "  结果分类: "
             << solveOutcomeName(result_.solveStatus,
                                 result_.hasIntegerSolution)
             << "\n";
        file << "  存在整数可行解: "
             << (result_.hasIntegerSolution ? "是" : "否") << "\n";
        if (result_.hasIntegerSolution) {
            file << "  最好可行目标值: " << std::fixed << std::setprecision(6)
                 << result_.totalProfit << "\n";
        }
        if (result_.bestBound < DBL_MAX / 2) {
            file << "  有效上界: " << result_.bestBound << "\n";
        }
        if (result_.relativeGap < DBL_MAX / 2) {
            file << "  相对Gap: " << result_.relativeGap << "\n";
        }
        file << "\n";

        if (result_.hasIntegerSolution) {
            file << (result_.solveStatus == SolveStatus::OPTIMAL
                         ? "认证最优解汇总:\n"
                         : "最好可行解汇总:\n");
            file << "  总利润: " << std::fixed << std::setprecision(2)
                 << result_.totalProfit << "\n";
            file << "  总碳排放: " << result_.carbonEmission << "\n";
            file << "  求解时间: " << result_.solveTime << " 秒\n\n";
        } else {
            file << "求解结果:\n";
            file << "  未获得整数可行解，不报告利润、碳排放或决策方案。\n";
            file << "  求解时间: " << std::fixed << std::setprecision(2)
                 << result_.solveTime << " 秒\n\n";
        }

        file << "网络决策:\n";
        file << "  开设DC数量: " << result_.numDCsOpen << "\n";
        file << "  服务零售商数量: " << result_.numRtsServed << "\n";
        if (baseProfit > 0) {
            file << "  基准方案利润（w=0）: " << baseProfit << "\n";
        }
        file << "\n";

        file << "DC详情:\n";
        for (int j = 0; j < inst.numDC; j++) {
            file << "  DC " << j << ":\n";
            bool isOpen = false;
            double p_j = 0, D_j = 0, Q_j = 0;
            std::vector<int> served;
            for (const auto& dcSol : result_.dcSolutions) {
                if (dcSol.dcIndex == j) {
                    isOpen = true;
                    p_j = dcSol.p;
                    for (int i = 0; i < inst.numRetailer; i++) {
                        if (dcSol.S[i] == 1) {
                            served.push_back(i);
                            D_j += inst.mu[i];
                        }
                    }
                    // 计算 Q_j*
                    double w_j = (j < (int)w.size()) ? w[j] : 0.0;
                    double inv = inst.h + inst.p * inst.hat_h * (1.0 - w_j);
                    if (inv > 0 && D_j > 0) {
                        Q_j = std::sqrt(2.0 * (inst.F[j] + inst.g[j]) * D_j / inv);
                    }
                    break;
                }
            }
            if (isOpen) {
                file << "    是否开设: 是\n";
                file << "    减排率 w_" << j << ": " << w[j] << "\n";
                file << "    批发价 p_" << j << ": " << p_j << "\n";
                file << "    服务总需求 D_" << j << ": " << D_j << "\n";
                file << "    最优订货量 Q_" << j << "*: " << Q_j << "\n";
                file << "    服务的零售商: [";
                for (size_t k = 0; k < served.size(); k++) {
                    if (k > 0) file << ", ";
                    file << served[k];
                }
                file << "]\n";
            } else {
                file << "    是否开设: 否\n";
            }
            file << "\n";
        }

        file << "碳交易:\n";
        file << "  初始碳配额 C: " << inst.C << "\n";
        file << "  实际碳排放 E: " << result_.carbonEmission << "\n";
        double e_plus = std::max(0.0, result_.carbonEmission - inst.C);
        double e_minus = std::max(0.0, inst.C - result_.carbonEmission);
        file << "  需要购买 e+: " << e_plus << "\n";
        file << "  可出售 e-: " << e_minus << "\n";
        file << "  净碳交易成本: " << inst.p * (result_.carbonEmission - inst.C) << "\n\n";

        // 时间分布 + 求解统计（复用 Monitor 生成的文本，与终端/run.log 完全统一）
        file << result_.timingReportText << "\n\n";
        file << result_.summaryText << "\n";

        file.close();
    }

    /** 保存收敛曲线 CSV */
    void saveConvergenceCSV(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[ResultWriter] 无法写入 " << path << std::endl;
            return;
        }
        file << "Generation,BestFitness,AvgFitness,WorstFitness,ElapsedTime" << std::endl;
        for (const auto& p : result_.convergence) {
            file << p.generation << ","
                 << std::fixed << std::setprecision(6)
                 << p.bestFitness << ","
                 << p.avgFitness << ","
                 << p.worstFitness << ","
                 << p.elapsedTime << std::endl;
        }
        file.close();
    }

    /** 保存单次实验的一行统一机器可读结果。 */
    void saveResultCSV(const std::string& path) const {
        saveExperimentCSV(path, std::vector<ExperimentResult>{result_});
    }

    /** 保存可直接合并分析的统一 CSV 结果表。 */
    static void saveExperimentCSV(
            const std::string& path,
            const std::vector<ExperimentResult>& results) {
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[ResultWriter] 无法写入 " << path << std::endl;
            return;
        }
        file
            << "experiment_name,outcome,inner_bp_status,has_integer_solution,"
            << "run_mode,num_dc,num_retailers,random_seed,delta,"
            << "investment_exponent,pricing_algorithm,pricing_max_cols_per_dc,"
            << "configured_mutation_rate,effective_mutation_rate,mutation_active,"
            << "total_profit,inner_bp_best_bound,inner_bp_relative_gap,"
            << "carbon_emission,"
            << "carbon_cap,e_plus,e_minus,net_carbon_trading_cost,"
            << "total_investment_cost,num_dc_open,num_retailers_served,"
            << "branch_nodes,pruned_nodes,cg_iterations,total_columns,"
            << "ga_generations,solve_time,ga_time,bp_time,cg_time,"
            << "pricing_time,cplex_build_time,cplex_solve_time,bb_time,best_w\n";
        file << std::setprecision(15);
        for (const auto& r : results) {
            file << csvEscape(r.experimentName) << ","
                 << (r.runMode == "ga"
                         ? (r.hasIntegerSolution
                                ? "HEURISTIC_BEST_FOUND"
                                : "NO_FEASIBLE_SOLUTION_FOUND")
                         : solveOutcomeName(
                               r.solveStatus, r.hasIntegerSolution))
                 << ","
                 << solveStatusName(r.solveStatus) << ","
                 << (r.hasIntegerSolution ? 1 : 0) << ","
                 << csvEscape(r.runMode) << ","
                 << r.numDC << "," << r.numRetailers << ","
                 << r.randomSeed << "," << r.delta << ","
                 << r.investmentExponent << ","
                 << r.pricingAlgorithm << "," << r.pricingMaxColsPerDC << ","
                 << r.configuredMutationRate << ","
                 << r.effectiveMutationRate << ","
                 << (r.runMode == "ga" ? 1 : 0) << ",";
            if (r.hasIntegerSolution) writeFinite(file, r.totalProfit);
            file << ",";
            writeFinite(file, r.bestBound);
            file << ",";
            writeFinite(file, r.relativeGap);
            file << ",";
            if (r.hasIntegerSolution) {
                file << r.carbonEmission << "," << r.carbonCap << ","
                     << r.ePlus << "," << r.eMinus << ","
                     << r.netCarbonTradingCost << ","
                     << r.totalInvestmentCost << ","
                     << r.numDCsOpen << "," << r.numRtsServed;
            } else {
                file << ",,,,,,,";
            }
            file << "," << r.totalBranchNodes << "," << r.prunedNodes << ","
                 << r.totalCGIterations << "," << r.totalColumnsGenerated << ","
                 << r.totalGAGenerations << "," << r.solveTime << ","
                 << r.timing.gaTime << "," << r.timing.bpTime << ","
                 << r.timing.cgTime << "," << r.timing.pricingTime << ","
                 << r.timing.cplexBuildTime << ","
                 << r.timing.cplexSolveTime << "," << r.timing.bbTime << ","
                 << csvEscape(vectorToString(r.bestW)) << "\n";
        }
    }

    /** 保存批量实验对比表格（含时间分布） */
    static void saveExperimentTable(const std::string& path,
                                     const std::vector<ExperimentResult>& results) {
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[ResultWriter] 无法写入 " << path << std::endl;
            return;
        }

        file << "===== 批量实验结果 =====\n\n";
        // 第一行：核心指标
        file << std::left
             << std::setw(16) << "名称"
             << std::setw(30) << "结果分类"
             << std::setw(6) << "#DC"
             << std::setw(6) << "#Rts"
             << std::setw(16) << "总利润"
             << std::setw(14) << "碳排放"
             << std::setw(12) << "e+购买"
             << std::setw(12) << "e-出售"
             << std::setw(10) << "时间(s)"
             << std::setw(8) << "#DC开"
             << std::setw(8) << "#R服务"
             << std::setw(8) << "节点数"
             << std::setw(8) << "CG迭代"
             << std::setw(10) << "GA时间"
             << std::setw(10) << "BP时间"
             << std::setw(10) << "CG时间"
             << std::setw(10) << "PS时间"
             << std::setw(10) << "BB时间" << std::endl;
        file << std::string(204, '-') << std::endl;

        for (const auto& r : results) {
            file << std::left << std::fixed << std::setprecision(2);
            file << std::setw(16) << r.experimentName
                 << std::setw(30)
                 << solveOutcomeName(r.solveStatus, r.hasIntegerSolution)
                 << std::setw(6) << r.numDC
                 << std::setw(6) << r.numRetailers
                 << std::setw(16) << r.totalProfit
                 << std::setw(14) << r.carbonEmission
                 << std::setw(12) << r.ePlus
                 << std::setw(12) << r.eMinus
                 << std::setw(10) << r.solveTime
                 << std::setw(8) << r.numDCsOpen
                 << std::setw(8) << r.numRtsServed
                 << std::setw(8) << r.totalBranchNodes
                 << std::setw(8) << r.totalCGIterations
                 << std::setw(10) << std::setprecision(4) << r.timing.gaTime
                 << std::setw(10) << r.timing.bpTime
                 << std::setw(10) << r.timing.cgTime
                 << std::setw(10) << r.timing.pricingTime
                 << std::setw(10) << r.timing.bbTime << std::endl;
        }

        file.close();
    }

private:
    static std::string csvEscape(const std::string& value) {
        if (value.find_first_of(",\"\n\r") == std::string::npos) {
            return value;
        }
        std::string escaped = "\"";
        for (char c : value) {
            if (c == '"') escaped += '"';
            escaped += c;
        }
        escaped += '"';
        return escaped;
    }

    static std::string vectorToString(const std::vector<double>& values) {
        std::ostringstream out;
        out << std::setprecision(15);
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) out << ";";
            out << values[i];
        }
        return out.str();
    }

    static void writeFinite(std::ofstream& file, double value) {
        if (std::isfinite(value) && std::abs(value) < DBL_MAX / 2) {
            file << value;
        }
    }

    ExperimentResult result_;
};

#endif // RESULT_WRITER_H
