#ifndef EXPERIMENT_RUNNER_H
#define EXPERIMENT_RUNNER_H

#include "../2_config/Config.h"
#include "../0_base/01_types/Instance.h"
#include "../0_base/01_types/Solution.h"
#include "../0_base/01_types/Types.h"
#include "../0_base/02_data/DataGenerator.h"
#include "../4_algorithm/05_GeneticAlgorithm.h"
#include "../4_algorithm/04_BranchAndPrice.h"
#include "../5_postprocessor/01_PostProcessor.h"
#include "../5_postprocessor/02_ResultWriter.h"
#include "../0_base/03_utils/Logger.h"
#include <string>
#include <iostream>
#include <chrono>
#include <sys/stat.h>
#include <fstream>

/**
 * ExperimentRunner — 第1层：实验运行引擎
 *
 * 封装"生成数据 → 基准方案 → GA求解 → 后处理 → 输出"的标准流程。
 * 启动器（main.cpp / run_experiments.cpp）只需要提供 Config，不需要关心具体流程。
 */

struct ExperimentResult {
    std::string name;
    int numDC;
    int numRetailers;
    double delta;
    double totalProfit;
    double carbonEmission;
    double solveTime;
    int numDCsOpen;
    int numRtsServed;
    double baseProfit;
    double baseEmission;
    int branchNodes;
    std::vector<double> bestW;
    double investCost;
};

class ExperimentRunner {
public:
    /**
     * 运行单次实验
     * @param config  实验配置
     * @param logger  日志器
     * @return 实验结果
     */
    static ExperimentResult run(const Config& config, Logger& logger) {
        ExperimentResult result;
        result.name = "exp";
        result.numDC = config.num_dc;
        result.numRetailers = config.num_retailers;
        result.delta = config.delta;

        auto start = std::chrono::steady_clock::now();

        // 生成实例
        logger.log(0, "程序启动");
        DataGenerator gen(config.random_seed);
        gen.setLegacyMode(config.legacy_mode);
        Instance inst = gen.generate(config);

        // 应用运输成本模式
        TransportCostFormula::useDirectDistance = config.transport_direct_distance;

        logger.log(0, "实例生成完成: #DC=" + std::to_string(inst.numDC)
                    + ", #零售商=" + std::to_string(inst.numRetailer));

        // 基准方案（w=0）
        logger.log(0, "计算基准方案（w=0）");
        std::vector<double> zero_w(inst.numDC, 0.0);

        BranchAndPrice bpBase;
        bpBase.setInstance(inst);
        bpBase.setConfig(config);
        bpBase.setLogger(&logger);

        Instance& mutableInst = const_cast<Instance&>(inst);
        mutableInst.setEmissionReductionRates(zero_w);

        double baseProfit = bpBase.solve(zero_w, "基准方案");

        Solution baseSolution = bpBase.getBestSolution();
        PostProcessor postProcBase(inst, baseSolution, zero_w);
        postProcBase.compute();
        double baseEmission = postProcBase.getCarbonResult().E;

        result.baseProfit = baseProfit;
        result.baseEmission = baseEmission;

        std::cout << "基准方案（w=0）利润: " << baseProfit
                  << "，碳排放: " << baseEmission << std::endl;

        // 遗传算法
        std::cout << "\n========== 运行遗传算法 ==========" << std::endl;
        auto gaStart = std::chrono::steady_clock::now();

        GeneticAlgorithm ga(config, inst);
        ga.setLogger(&logger);
        Solution bestSolution = ga.run();

        auto gaEnd = std::chrono::steady_clock::now();
        double gaTime = std::chrono::duration<double>(gaEnd - gaStart).count();

        // 收集结果
        result.totalProfit = bestSolution.totalProfit;
        result.solveTime = gaTime;
        result.numDCsOpen = bestSolution.numDCsOpen;
        result.numRtsServed = bestSolution.numRtsServed;
        result.bestW = ga.getBestW();
        result.investCost = bestSolution.totalInvestmentCost;
        result.branchNodes = bpBase.getTotalNodes();

        // 后处理
        PostProcessor postProc(inst, bestSolution, ga.getBestW());
        postProc.compute();
        result.carbonEmission = postProc.getCarbonResult().E;

        // 输出结果
        std::cout << "\n========== 最终结果 ==========" << std::endl;
        std::cout << "总利润: " << bestSolution.totalProfit << std::endl;
        std::cout << "碳排放: " << result.carbonEmission << std::endl;
        std::cout << "求解时间(秒): " << gaTime << std::endl;
        std::cout << "#DCs Open: " << bestSolution.numDCsOpen << std::endl;
        std::cout << "#Rts Served: " << bestSolution.numRtsServed << std::endl;

        // 保存收敛曲线
        mkdir(config.output_dir.c_str(), 0755);
        std::string csvFile = config.output_dir + "/convergence.csv";
        SolutionHelper::saveConvergenceCSV(csvFile, ga.getConvergence());
        std::cout << "收敛曲线已保存至: " << csvFile << std::endl;

        // 生成报告
        std::string reportFile = config.output_dir + "/report.txt";
        ResultWriter::writeReport(reportFile, inst, bestSolution, ga.getBestW(),
                                  baseProfit, gaTime, postProc);
        std::cout << "完整决策方案报告已保存至: " << reportFile << std::endl;

        return result;
    }
};

#endif // EXPERIMENT_RUNNER_H
