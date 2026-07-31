#ifndef MONITOR_H
#define MONITOR_H

#include "../10_common/Types.h"
#include "../5_result/ExperimentResult.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/**
 * 监控模块（Monitor）— 运行时仪表盘 + 时间统计中心
 *
 * 管运行时的状态和进度。各模块通过 report* 方法上报状态，
 * 其他模块通过 get* 方法查询当前状态。
 *
 * 时间统计采用层级累加模式——每个模块自记内部耗时，
 * 最终通过 buildTimingReport() 汇总为 TimingReport 结构。
 *
 * 与 Logger 的区别：
 *   Logger 写流水账（一行行日志），Monitor 记状态（当前最优值、代数等）。
 */
class Monitor {
public:
    Monitor() {
        startTime_ = std::chrono::steady_clock::now();
        phaseStartTime_ = startTime_;
    }

    // ===== 各模块上报 =====
    void reportPhase(const std::string& phase) {
        currentPhase_ = phase;
        phaseStartTime_ = std::chrono::steady_clock::now();
    }

    void reportGeneration(int gen, double best, double avg,
                          double worst, const std::vector<double>& bestW) {
        generation_ = gen;
        bestFitness_ = best;
        avgFitness_ = avg;
        worstFitness_ = worst;
        bestW_ = bestW;
        convergenceHistory_.push_back({gen, best, avg, worst, getElapsedSeconds()});
    }

    void reportIndividual(int idx, double fitness) {
        // 可选：记录个体级别的信息
    }

    void reportCGIteration(int iter, int numCols) {
        columnGenIterations_ = iter;
        totalColumns_ = numCols;
    }

    void reportBBNodes(int total, int pruned,
                       int processed = 0, int remainingActive = 0) {
        totalBranchNodes_ = total;
        prunedNodes_ = pruned;
        processedNodes_ = processed;
        remainingActiveNodes_ = remainingActive;
    }

    // ===== 详细时间上报（按层级）=====

    /** GA 层：GA 搜索总时间 + 各子操作时间 */
    void reportGATiming(double gaTime,
                         double selectTime, double crossoverTime,
                         double mutateTime, double decodeTime,
                         double evalTime) {
        gaTime_ = gaTime;
        gaSelectTime_ = selectTime;
        gaCrossoverTime_ = crossoverTime;
        gaMutateTime_ = mutateTime;
        gaDecodeTime_ = decodeTime;
        gaEvalTime_ = evalTime;
    }

    /** BP 层：分支定价总时间（墙钟口径） */
    void reportBPTiming(double bpTime) {
        bpTime_ = bpTime;
    }

    /** BP 并行管理开销（诊断口径） */
    void reportBPParallelOverhead(double forkTime,
                                  double pipeIoTime,
                                  double waitTime,
                                  double mergeTime) {
        bpForkTime_ = forkTime;
        bpPipeIoTime_ = pipeIoTime;
        bpWaitTime_ = waitTime;
        bpMergeTime_ = mergeTime;
    }

    /** CG 层：列生成总时间 + CPLEX 细分时间 */
    void reportCGTiming(double cgTime,
                         double cplexBuildTime, double cplexSolveTime) {
        cgTime_ = cgTime;
        cplexBuildTime_ = cplexBuildTime;
        cplexSolveTime_ = cplexSolveTime;
    }

    /** PS 层：定价子问题总时间 */
    void reportPricingTiming(double pricingTime) {
        pricingTime_ = pricingTime;
    }

    /** BB 层：分支定界总时间 */
    void reportBBTiming(double bbTime) {
        bbTime_ = bbTime;
    }

    /** 后处理耗时 */
    void reportPostTiming(double postTime) {
        postTime_ = postTime;
    }

    /** 日志耗时（粗略） */
    void reportLogTiming(double logTime) {
        logTime_ = logTime;
    }

    // ===== 查询接口 =====
    double bestFitness() const { return bestFitness_; }
    double avgFitness() const { return avgFitness_; }
    double worstFitness() const { return worstFitness_; }
    int currentGeneration() const { return generation_; }
    std::string currentPhase() const { return currentPhase_; }

    double getElapsedSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - startTime_).count();
    }

    double getPhaseElapsedSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - phaseStartTime_).count();
    }

    double estimatedProgress() const {
        if (totalGenerations_ <= 0) return 0.0;
        return std::min(1.0, (double)generation_ / totalGenerations_);
    }

    const std::vector<ConvergencePoint>& convergenceHistory() const {
        return convergenceHistory_;
    }

    int totalBranchNodes() const { return totalBranchNodes_; }
    int prunedNodes() const { return prunedNodes_; }
    int processedNodes() const { return processedNodes_; }
    int remainingActiveNodes() const { return remainingActiveNodes_; }
    int columnGenIterations() const { return columnGenIterations_; }
    int totalColumns() const { return totalColumns_; }

    // ===== 时间统计查询 =====
    double gaTime() const { return gaTime_; }
    double bpTime() const { return bpTime_; }
    double cgTime() const { return cgTime_; }
    double pricingTime() const { return pricingTime_; }
    double bbTime() const { return bbTime_; }

    // ===== 构建完整的 TimingReport =====
    TimingReport buildTimingReport() const {
        TimingReport t;
        t.solveTime = getElapsedSeconds();
        t.gaTime = gaTime_;
        t.gaSelectTime = gaSelectTime_;
        t.gaCrossoverTime = gaCrossoverTime_;
        t.gaMutateTime = gaMutateTime_;
        t.gaDecodeTime = gaDecodeTime_;
        t.gaEvalTime = gaEvalTime_;

        t.bpTime = bpTime_;
        t.bpForkTime = bpForkTime_;
        t.bpPipeIoTime = bpPipeIoTime_;
        t.bpWaitTime = bpWaitTime_;
        t.bpMergeTime = bpMergeTime_;
        t.bpOtherTime = std::max(
            0.0,
            bpTime_ - cgTime_ - bbTime_ - bpForkTime_ - bpPipeIoTime_ - bpWaitTime_ - bpMergeTime_);

        t.cgTime = cgTime_;
        t.cplexBuildTime = cplexBuildTime_;
        t.cplexSolveTime = cplexSolveTime_;
        t.pricingTime = pricingTime_;
        t.bbTime = bbTime_;
        t.postTime = postTime_;
        t.logTime = logTime_;
        return t;
    }

    // ===== 时间管理 =====
    void reset() {
        startTime_ = std::chrono::steady_clock::now();
        phaseStartTime_ = startTime_;
        generation_ = 0;
        bestFitness_ = -DBL_MAX;
        avgFitness_ = 0.0;
        worstFitness_ = 0.0;
        bestW_.clear();
        totalBranchNodes_ = 0;
        prunedNodes_ = 0;
        processedNodes_ = 0;
        remainingActiveNodes_ = 0;
        columnGenIterations_ = 0;
        totalColumns_ = 0;
        gaTime_ = 0.0;
        bpTime_ = 0.0;
        bpForkTime_ = 0.0;
        bpPipeIoTime_ = 0.0;
        bpWaitTime_ = 0.0;
        bpMergeTime_ = 0.0;
        cgTime_ = 0.0;
        pricingTime_ = 0.0;
        bbTime_ = 0.0;
        gaSelectTime_ = 0.0;
        gaCrossoverTime_ = 0.0;
        gaMutateTime_ = 0.0;
        gaDecodeTime_ = 0.0;
        gaEvalTime_ = 0.0;
        cplexBuildTime_ = 0.0;
        cplexSolveTime_ = 0.0;
        postTime_ = 0.0;
        logTime_ = 0.0;
        totalGAGenerations_ = 0;
        convergenceHistory_.clear();
    }

    void setTotalGenerations(int total) { totalGenerations_ = total; }
    void reportGAGenerations(int total) { totalGAGenerations_ = total; }

    // ===== 构建时间分布文本（双报表：墙钟树 + 诊断树） =====
    // 与 printTimingReport 输出一致，但返回字符串，便于同时写入终端和日志文件。
    std::string formatTimingReport() const {
        const TimingReport t = buildTimingReport();
        std::ostringstream os;
        os << std::fixed << std::setprecision(4);

        auto residual = [](double parent, std::initializer_list<double> children) {
            double sum = 0.0;
            for (double c : children) {
                sum += c;
            }
            return std::max(0.0, parent - sum);
        };

        auto print_node = [&os](const std::string& name, double value,
                                double parent, int indent) {
            if (parent <= 0.0) {
                return;
            }
            os << std::string(indent, ' ')
               << std::left << std::setw(24) << name
               << std::right << std::setw(10) << value << "s ("
             << std::setw(6) << std::setprecision(1)
               << (value / parent * 100.0) << "%)" << std::setprecision(4)
               << "\n";
        };

        const double total_other = residual(t.solveTime, {t.gaTime, t.postTime, t.logTime});
        const double ga_other = residual(
            t.gaTime,
            {t.gaSelectTime, t.gaCrossoverTime, t.gaMutateTime, t.gaDecodeTime, t.gaEvalTime});
        const double eval_other = residual(t.gaEvalTime, {t.bpTime});
        const double bp_wall_other = residual(t.bpTime, {t.cgTime, t.bbTime});
        const double cg_other = residual(t.cgTime, {t.cplexBuildTime, t.cplexSolveTime, t.pricingTime});

        os << "\n========== 时间分布报告（墙钟树） ==========" << "\n";
        os << "总耗时                  " << t.solveTime << "s (100%)" << "\n";
        print_node("GA 搜索", t.gaTime, t.solveTime, 2);
        print_node("总耗时 other", total_other, t.solveTime, 2);
        print_node("后处理", t.postTime, t.solveTime, 2);
        print_node("日志输出", t.logTime, t.solveTime, 2);

        print_node("选择", t.gaSelectTime, t.gaTime, 4);
        print_node("交叉", t.gaCrossoverTime, t.gaTime, 4);
        print_node("变异", t.gaMutateTime, t.gaTime, 4);
        print_node("解码", t.gaDecodeTime, t.gaTime, 4);
        print_node("适应度评估", t.gaEvalTime, t.gaTime, 4);
        print_node("GA other", ga_other, t.gaTime, 4);

        print_node("分支定价(BP)", t.bpTime, t.gaEvalTime, 6);
        print_node("评估 other", eval_other, t.gaEvalTime, 6);

        print_node("列生成(CG)", t.cgTime, t.bpTime, 8);
        print_node("分支定界(BB)", t.bbTime, t.bpTime, 8);
        print_node("BP other(墙钟)", bp_wall_other, t.bpTime, 8);

        print_node("CPLEX构建", t.cplexBuildTime, t.cgTime, 10);
        print_node("CPLEX求解", t.cplexSolveTime, t.cgTime, 10);
        print_node("定价子问题(PS)", t.pricingTime, t.cgTime, 10);
        print_node("CG other", cg_other, t.cgTime, 10);

        os << "\n========== 时间分布报告（诊断树） ==========" << "\n";
        os << "BP墙钟总时间             " << t.bpTime << "s (100%)" << "\n";
        print_node("列生成(CG)", t.cgTime, t.bpTime, 2);
        print_node("分支定界(BB)", t.bbTime, t.bpTime, 2);
        print_node("并行fork", t.bpForkTime, t.bpTime, 2);
        print_node("并行pipe I/O", t.bpPipeIoTime, t.bpTime, 2);
        print_node("并行wait", t.bpWaitTime, t.bpTime, 2);
        print_node("并行merge", t.bpMergeTime, t.bpTime, 2);
        print_node("BP未归类(other)", t.bpOtherTime, t.bpTime, 2);

        print_node("CPLEX构建", t.cplexBuildTime, t.cgTime, 4);
        print_node("CPLEX求解", t.cplexSolveTime, t.cgTime, 4);
        print_node("定价子问题(PS)", t.pricingTime, t.cgTime, 4);
        print_node("CG other", cg_other, t.cgTime, 4);
        os << "==========================================";
        return os.str();
    }

    // ===== 构建统计摘要文本 =====
    std::string formatSummary() const {
        std::ostringstream os;
        os << std::fixed << std::setprecision(4);
        os << "===== 求解统计 =====" << "\n";
        os << "分支决策次数（旧branch_nodes口径）: "
           << totalBranchNodes_ << "\n";
        os << "已处理节点数: " << processedNodes_ << "\n";
        os << "剪枝节点数: " << prunedNodes_ << "\n";
        os << "累计未处理活跃节点数: "
           << remainingActiveNodes_ << "\n";
        os << "列生成迭代次数: " << columnGenIterations_ << "\n";
        os << "总列数: " << totalColumns_ << "\n";
        // 派生指标：拆分"迭代总数"到底来自"节点多"还是"每节点迭代多"
        if (processedNodes_ > 0) {
            double iterPerNode =
                static_cast<double>(columnGenIterations_) / processedNodes_;
            double colPerNode =
                static_cast<double>(totalColumns_) / processedNodes_;
            os << "平均每节点CG迭代: " << iterPerNode << "\n";
            os << "平均每节点生成列: " << colPerNode << "\n";
        }
        if (columnGenIterations_ > 0) {
            double colPerIter =
                static_cast<double>(totalColumns_)/ columnGenIterations_;
            os << "平均每迭代加列: " << colPerIter << "\n";
        }
        os << "=========================";
        return os.str();
    }

    // ===== 打印时间分布（双报表：墙钟树 + 诊断树） =====
    void printTimingReport() const {
        std::cout << formatTimingReport()<< std::endl;
    }

    // ===== 打印统计摘要 =====
    void printSummary() const {
        std::cout << formatSummary() << std::endl;
    }

private:
    // 运行状态
    std::string currentPhase_;
    int generation_ = 0;
    int totalGenerations_ = 50;
    double bestFitness_ = -DBL_MAX;
    double avgFitness_ = 0.0;
    double worstFitness_ = 0.0;
    std::vector<double> bestW_;

    // 算法统计
    int totalBranchNodes_ = 0;
    int prunedNodes_ = 0;
    int processedNodes_ = 0;
    int remainingActiveNodes_ = 0;
    int columnGenIterations_ = 0;
    int totalColumns_ = 0;

    // ===== 时间统计（详细层级） =====

    // GA 层
    double gaTime_ = 0.0;
    double gaSelectTime_ = 0.0;
    double gaCrossoverTime_ = 0.0;
    double gaMutateTime_ = 0.0;
    double gaDecodeTime_ = 0.0;
    double gaEvalTime_ = 0.0;

    // BP 层
    double bpTime_ = 0.0;
    double bpForkTime_ = 0.0;
    double bpPipeIoTime_ = 0.0;
    double bpWaitTime_ = 0.0;
    double bpMergeTime_ = 0.0;

    // CG 层
    double cgTime_ = 0.0;
    double cplexBuildTime_ = 0.0;
    double cplexSolveTime_ = 0.0;

    // PS 层
    double pricingTime_ = 0.0;

    // BB 层
    double bbTime_ = 0.0;

    // 其他
    double postTime_ = 0.0;
    double logTime_ = 0.0;
    int totalGAGenerations_ = 0;

    // 收敛曲线
    std::vector<ConvergencePoint> convergenceHistory_;

    // 基准时间
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point phaseStartTime_;
};

#endif // MONITOR_H
