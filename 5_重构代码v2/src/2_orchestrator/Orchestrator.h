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
#include "../7_formula/05_InvestmentCost.h"
#include "../8_solver/05_GeneticAlgorithm.h"
#include "../8_solver/04_BranchAndPrice.h"
#include "../9_postprocessor/01_PostProcessor.h"
#include <string>
#include <iostream>
#include <sstream>
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

    /**
     * 初始化
     *
     * @param configPath 配置文件路径
     * @param externalOutputDir 外部指定的输出目录（批量实验用）。
     *        非空时直接使用该目录，不再创建 single 目录；
     *        为空时按单次运行创建 results/single/ 目录。
     */
    void initialize(const std::string& configPath,
                    const std::string& externalOutputDir = "") {
        // 读配置
        config_.loadFromFile(configPath);

        // 确定输出目录：批量实验传入 entryDir（落到 results/experiment/），
        // 单次运行则创建 results/single/ 目录，二者互不干扰。
        if (!externalOutputDir.empty()) {
            outputDir_ = externalOutputDir;
            if (outputDir_.back() != '/') outputDir_ += '/';
        } else {
            std::string configName =
                configPath.substr(configPath.find_last_of("/") + 1);
            configName = configName.substr(0, configName.find_last_of("."));
            outputDir_ = OutputManager::createSingleRunDir(configName);
        }
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
        bp.setMonitor(&monitor_);

        // 求解
        auto start = steady_clock::now();
        double profit = bp.solve(config_.fixed_w, "固定w求解");
        auto end = steady_clock::now();
        double solveTime = duration<double>(end - start).count();
        monitor_.reportBPTiming(solveTime);

        // 收集结果
        Solution sol = bp.getBestSolution();
        sol.totalProfit = profit;
        sol.w = config_.fixed_w;

        double emission = 0.0;
        std::unique_ptr<PostProcessor> postProc;
        if (sol.hasIntegerSolution) {
            postProc.reset(new PostProcessor(
                *instance_, sol, config_.fixed_w));
            postProc->compute();
            emission = postProc->getCarbonResult().E;
        }

        // 输出结果
        outputResults(sol, config_.fixed_w, profit, emission, solveTime, 0);

        // 后处理明细（逐 DC 的 w/p/D/Q* + 碳交易 e±）打到终端
        if (postProc) postProc->print();

        // 打印时间分布报告（终端 + run.log 同步）
        emitTimingAndSummary();
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
        outputResults(bestSolution, bestW, profit, emission, gaTime, 0,
                      ga.getFitnessRequests(), ga.getUniqueChromosomes(),
                      ga.getRepeatedChromosomeRequests(),
                      ga.getActualFitnessCacheHits());

        // 后处理明细（逐 DC 的 w/p/D/Q* + 碳交易 e±）打到终端
        postProc.print();

        // 打印时间分布报告（终端 + run.log 同步）
        emitTimingAndSummary();
    }

    /**
     * 输出时间分布报告和统计摘要。
     *
     * 终端与 run.log 同步：时间分布树既打印到控制台，也写入日志文件，
     * 保证工作台输出（run.log）与终端内容一致。
     */
    void emitTimingAndSummary() {
        const std::string timing = monitor_.formatTimingReport();
        const std::string summary = monitor_.formatSummary();

        // 终端输出
        std::cout << timing << std::endl;
        std::cout << summary << std::endl;

        // 写入 run.log（临时关闭终端模式，避免重复打印）
        logMultilineToFile(timing);
        logMultilineToFile(summary);
    }

    /** 将多行文本逐行写入 run.log（仅文件，不重复输出终端） */
    void logMultilineToFile(const std::string& text) {
        const bool prevConsole = logger_.getConsoleMode();
        logger_.setConsoleMode(false);
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            logger_.raw(line);
        }
        logger_.setConsoleMode(prevConsole);
    }

    /** 统一输出结果 */
    void outputResults(const Solution& sol, const std::vector<double>& w,
                        double profit, double emission, double solveTime,
                        double baseProfit,
                        long long fitnessRequests = 0,
                        long long uniqueChromosomes = 0,
                        long long repeatedChromosomeRequests = 0,
                        long long actualFitnessCacheHits = 0) {
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
        resultWriter_.setConfigMeta(config_);
        int gaGens = static_cast<int>(monitor_.convergenceHistory().size());
        resultWriter_.collectFrom(monitor_, w, profit, emission, solveTime,
                                   dcOpen, rtServed, monitor_.totalBranchNodes(),
                                   gaGens, instance_->C);
        resultWriter_.setSolution(sol.dcSolutions);
        resultWriter_.setSolveCertification(sol);
        resultWriter_.setGAFitnessStats(
            fitnessRequests, uniqueChromosomes,
            repeatedChromosomeRequests, actualFitnessCacheHits);
        double totalInvestmentCost = 0.0;
        if (sol.hasIntegerSolution) {
            for (const auto& dcSol : sol.dcSolutions) {
                if (dcSol.dcIndex < 0 || dcSol.dcIndex >= instance_->numDC) {
                    continue;
                }
                double demand = 0.0;
                for (int i = 0;
                     i < instance_->numRetailer
                         && i < static_cast<int>(dcSol.S.size());
                     ++i) {
                    if (dcSol.S[i] == 1) demand += instance_->mu[i];
                }
                if (demand <= 0.0) continue;
                const int j = dcSol.dcIndex;
                const double wj =
                    j < static_cast<int>(w.size()) ? w[j] : 0.0;
                totalInvestmentCost += InvestmentCostFormula::compute(
                    *instance_, j, wj, demand,
                    config_.use_sqrt_investment,
                    config_.investment_exponent);
            }
        }
        resultWriter_.setTotalInvestmentCost(totalInvestmentCost);

        // 保存报告
        std::string reportFile = outputDir_ + "report.txt";
        resultWriter_.saveReport(reportFile, *instance_, w, baseProfit, config_);
        std::cout << "报告已保存至: " << reportFile << std::endl;

        // 保存收敛曲线
        std::string csvFile = outputDir_ + "convergence.csv";
        resultWriter_.saveConvergenceCSV(csvFile);
        std::cout << "收敛曲线已保存至: " << csvFile << std::endl;

        std::string resultCsvFile = outputDir_ + "result.csv";
        resultWriter_.saveResultCSV(resultCsvFile);
        std::cout << "统一结果表已保存至: " << resultCsvFile << std::endl;

        // 控制台输出摘要
        std::cout << "\n========== 最终结果 ==========" << std::endl;
        std::cout << "求解状态: " << solveStatusName(sol.solveStatus) << std::endl;
        std::cout << "结果分类: "
                  << solveOutcomeName(sol.solveStatus,
                                      sol.hasIntegerSolution)
                  << std::endl;
        if (sol.bestBound < DBL_MAX / 2) {
            std::cout << "有效上界: " << sol.bestBound
                      << " | 相对Gap: " << sol.relativeGap << std::endl;
        }
        if (sol.hasIntegerSolution) {
            std::cout << "总利润: " << profit << std::endl;
            std::cout << "碳排放: " << emission << std::endl;
        } else {
            std::cout << "未获得整数可行解，未报告利润和决策方案。" << std::endl;
        }
        std::cout << "求解时间(秒): " << solveTime << std::endl;
        std::cout << "#DCs Open: " << dcOpen << std::endl;
        std::cout << "#Rts Served: " << rtServed << std::endl;

        // 碳交易量（e+ 需购买 / e- 可出售），直观反映减排效果
        double cCap = instance_->C;
        double ePlus = std::max(0.0, emission - cCap);
        double eMinus = std::max(0.0, cCap - emission);
        std::cout << "碳配额 C: " << cCap
                  << " | e+ 购买: " << ePlus
                 << " | e- 出售: " << eMinus << std::endl;
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
