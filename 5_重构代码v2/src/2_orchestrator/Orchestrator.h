#ifndef ORCHESTRATOR_H
#define ORCHESTRATOR_H

#include "../10_common/Config.h"
#include "../10_common/Instance.h"
#include "../10_common/Types.h"
#include "../10_common/Solution.h"
#include "../10_common/Random.h"
#include "../10_common/OutputManager.h"
#include "../4_logger/Logger.h"
#include "../3_monitor/Monitor.h"
#include "../5_result/ExperimentResult.h"
#include "../5_result/ResultWriter.h"
#include "../6_preprocessor/DataGenerator.h"
#include "../8_solver/05_GeneticAlgorithm.h"
#include "../8_solver/04_BranchAndPrice.h"
#include "../9_postprocessor/01_PostProcessor.h"
#include <string>
#include <iostream>
#include <memory>
#include <chrono>

/**
 * 编排模块（Orchestrator）⭐
 *
 * 三阶段生命周期：
 *   initialize(configPath) → 读配置、创建各模块、生成数据
 *   run()                  → 求解（GA+BP）/（固定w BP）→ 后处理 → 输出
 *   finalize()             → 收尾、输出摘要
 */
class Orchestrator {
public:
    Orchestrator() = default;
    ~Orchestrator() = default;

    /** 初始化 */
    void initialize(const std::string& configPath) {
        // 读配置
        config_.loadFromFile(configPath);

        // 创建输出目录
        std::string configName = configPath.substr(configPath.find_last_of("/") + 1);
        configName = configName.substr(0, configName.find_last_of("."));
        outputDir_ = OutputManager::createSingleRunDir(configName);
        config_.output_dir = outputDir_;
        config_.log_file = outputDir_ + "run.log";

        // 创建 Logger
        logger_.init(config_.log_file, config_.verbose);

        // 创建 Monitor
        monitor_.reset();
        monitor_.setTotalGenerations(config_.max_generation);

        // 生成数据
        DataGenerator gen(config_.random_seed);
        gen.setLegacyMode(config_.legacy_mode);
        instance_ = std::make_shared<Instance>(gen.generate(config_));
        std::cout << "[GA] #DC=" << instance_->numDC
                  << ", #零售商=" << instance_->numRetailer << std::endl;

        // 设置运输成本模式
        TransportCostFormula::useDirectDistance = config_.transport_direct_distance;

        // 创建求解器（不包含 GA 和 BP 的初始化，在 run 中按需创建）
        logger_.progress(0, "初始化完成，规模 " + std::to_string(instance_->numDC)
                        + "×" + std::to_string(instance_->numRetailer));
    }

    /** 运行 */
    void run() {
        using namespace std::chrono;

        if (config_.run_mode == "fixed_w") {
            // === 固定 w 模式：只跑 BP，不走 GA ===
            runFixedW();
        } else {
            // === 标准模式：完整 GA+BP ===
            runGA();
        }
    }

    /** 收尾 */
    void finalize() {
        double elapsed = monitor_.getElapsedSeconds();
        std::cout << "\n========== 运行完成 ==========" << std::endl;
        std::cout << "总耗时: " << elapsed << "s" << std::endl;
        std::cout << "结果保存至: " << outputDir_ << std::endl;
    }

    const Monitor& getMonitor() const { return monitor_; }
    const ExperimentResult& getResult() const { return resultWriter_.getResult(); }
    const std::string& getOutputDir() const { return outputDir_; }

private:
    /** 固定 w 模式 */
    void runFixedW() {
        using namespace std::chrono;

        logger_.progress(0, "固定w模式，w = [" + wToString(config_.fixed_w) + "]");

        // 设置 w
        instance_->setEmissionReductionRates(config_.fixed_w);

        // 创建 BP
        BranchAndPrice bp;
        bp.setInstance(*instance_);
        bp.setConfig(config_);
        bp.setLogger(&logger_);

        // 求解
        auto start = steady_clock::now();
        double profit = bp.solve(config_.fixed_w, "固定w求解");
        auto end = steady_clock::now();
        double solveTime = duration<double>(end - start).count();

        // 收集结果
        Solution sol = bp.getBestSolution();
        sol.totalProfit = profit;
        sol.w = config_.fixed_w;

        // 后处理
        PostProcessor postProc(*instance_, sol, config_.fixed_w);
        postProc.compute();
        double emission = postProc.getCarbonResult().E;

        // 输出结果
        outputResults(sol, config_.fixed_w, profit, emission, solveTime, 0);
    }

    /** 标准 GA+BP 模式 */
    void runGA() {
        using namespace std::chrono;

        logger_.progress(0, "遗传算法开始，种群大小=" + std::to_string(config_.population_size)
                    + "，最大代数=" + std::to_string(config_.max_generation)
                    + "，编码位数=" + std::to_string(config_.chromosome_length));

        // 创建 GA
        GeneticAlgorithm ga(config_, *instance_);
        ga.setLogger(&logger_);
        ga.setMonitor(&monitor_);

        // 运行 GA
        auto start = steady_clock::now();
        Solution bestSolution = ga.run();
        auto end = steady_clock::now();
        double gaTime = duration<double>(end - start).count();

        // 收集 GA 结果
        double profit = bestSolution.totalProfit;
        std::vector<double> bestW = ga.getBestW();
        const auto& convergence = ga.getConvergence();

        // 手动记录收敛曲线到 Monitor
        for (const auto& cp : convergence) {
            monitor_.reportGeneration(cp.generation, cp.bestFitness,
                                       cp.avgFitness, cp.worstFitness, bestW);
        }
        monitor_.reportGAGenerations(static_cast<int>(convergence.size()));

        // 后处理（计时）
        auto postStart = steady_clock::now();
        PostProcessor postProc(*instance_, bestSolution, bestW);
        postProc.compute();
        auto postEnd = steady_clock::now();
        double postTime = duration<double>(postEnd - postStart).count();
        double emission = postProc.getCarbonResult().E;
        monitor_.reportPostTiming(postTime);

        // 输出结果
        outputResults(bestSolution, bestW, profit, emission, gaTime, 0);

        // 打印时间分布报告
        monitor_.printTimingReport();
        monitor_.printSummary();
    }

    /** 统一输出结果 */
    void outputResults(const Solution& sol, const std::vector<double>& w,
                        double profit, double emission, double solveTime,
                        double baseProfit) {
        // 统计
        int dcOpen = 0;
        for (const auto& dcSol : sol.dcSolutions) {
            if (dcSol.p > 0 || dcSol.profit != 0) {
                // 检查是否有零售商被服务
                int served = 0;
                for (int s : dcSol.S) if (s == 1) served++;
                if (served > 0) dcOpen++;
            }
        }

        // 计算服务的零售商数
        int rtServed = 0;
        for (const auto& dcSol : sol.dcSolutions) {
            for (int s : dcSol.S) if (s == 1) rtServed++;
        }

        // 填充 ResultWriter
        resultWriter_.setMeta("exp", instance_->numDC, instance_->numRetailer,
                               config_.delta, config_.random_seed);
        int gaGens = static_cast<int>(monitor_.convergenceHistory().size());
        resultWriter_.collectFrom(monitor_, w, profit, emission, solveTime,
                                   dcOpen, rtServed, monitor_.totalBranchNodes(),
                                   gaGens);
        resultWriter_.setSolution(sol.dcSolutions);

        // 保存报告
        std::string reportFile = outputDir_ + "report.txt";
        resultWriter_.saveReport(reportFile, *instance_, w, baseProfit);
        std::cout << "报告已保存至: " << reportFile << std::endl;

        // 保存收敛曲线
        std::string csvFile = outputDir_ + "convergence.csv";
        resultWriter_.saveConvergenceCSV(csvFile);
        std::cout << "收敛曲线已保存至: " << csvFile << std::endl;

        // 控制台输出摘要
        std::cout << "\n========== 最终结果 ==========" << std::endl;
        std::cout << "总利润: " << profit << std::endl;
        std::cout << "碳排放: " << emission << std::endl;
        std::cout << "求解时间(秒): " << solveTime << std::endl;
        std::cout << "#DCs Open: " << dcOpen << std::endl;
        std::cout << "#Rts Served: " << rtServed << std::endl;
    }

    static std::string wToString(const std::vector<double>& w) {
        std::string s;
        for (size_t i = 0; i < w.size(); i++) {
            if (i > 0) s += ", ";
            s += std::to_string(w[i]);
        }
        return s;
    }

private:
    Config config_;
    Logger logger_;
    Monitor monitor_;
    ResultWriter resultWriter_;
    std::shared_ptr<Instance> instance_;
    std::string outputDir_;
};

#endif // ORCHESTRATOR_H
