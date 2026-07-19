#ifndef MONITOR_H
#define MONITOR_H

#include "../10_common/Types.h"
#include "../5_result/ExperimentResult.h"
#include <vector>
#include <string>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>

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

    void reportBBNodes(int total, int pruned) {
        totalBranchNodes_ = total;
        prunedNodes_ = pruned;
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

    /** BP 层：分支定价总时间 */
    void reportBPTiming(double bpTime) {
        bpTime_ = bpTime;
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
        columnGenIterations_ = 0;
        totalColumns_ = 0;
        gaTime_ = 0.0;
        bpTime_ = 0.0;
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

    // ===== 打印时间分布（树形缩进） =====
    void printTimingReport() const {
        double total = getElapsedSeconds();
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "\n========== 时间分布报告 ==========" << std::endl;
        std::cout << "总耗时:          " << total << "s (100%)" << std::endl;
        // 调试信息已移除
        auto printLine = [total](const std::string& name, double t) {
            if (total > 0) {
                std::cout << "  " << std::left << std::setw(18) << name
                          << std::right << std::setw(10) << std::setprecision(4) << t << "s ("
                          << std::setw(5) << std::setprecision(1) << (t / total * 100) << "%)"
                          << std::endl;
            }
        };
        printLine("GA 搜索", gaTime_);
        printLine("├─ 选择", gaSelectTime_);
        printLine("├─ 交叉", gaCrossoverTime_);
        printLine("├─ 变异", gaMutateTime_);
        printLine("├─ 解码", gaDecodeTime_);
        printLine("├─ 适应度评估", gaEvalTime_);
        printLine("│  └─ 分支定价(BP)", bpTime_);
        printLine("│     ├─ 列生成(CG)", cgTime_);
        printLine("│     │  ├─ CPLEX构建", cplexBuildTime_);
        printLine("│     │  ├─ CPLEX求解", cplexSolveTime_);
        printLine("│     │  └─ 定价子问题(PS)", pricingTime_);
        printLine("│     └─ 分支定界(BB)", bbTime_);
        printLine("后处理", postTime_);
        printLine("日志输出", logTime_);
        std::cout << "==================================" << std::endl;
    }

    // ===== 打印统计摘要 =====
    void printSummary() const {
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "===== 求解统计 =====" << std::endl;
        std::cout << "总分支节点数: " << totalBranchNodes_ << std::endl;
        std::cout << "剪枝节点数: " << prunedNodes_ << std::endl;
        std::cout << "列生成迭代次数: " << columnGenIterations_ << std::endl;
        std::cout << "总列数: " << totalColumns_ << std::endl;
        std::cout << "=========================" << std::endl;
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
