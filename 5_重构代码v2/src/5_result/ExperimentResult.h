#ifndef EXPERIMENT_RESULT_H
#define EXPERIMENT_RESULT_H

#include "../10_common/Types.h"
#include "../10_common/Solution.h"
#include <string>
#include <vector>

/**
 * 各阶段时间消耗统计（用于性能分析、对比优化效果）
 *
 * 层级结构（用时最长的在最外层）：
 *   solveTime        总求解时间
 *   ├── gaTime       GA 搜索时间（含内部所有 BP 调用）
 *   │   ├── gaSelectTime    选择操作耗时
 *   │   ├── gaCrossoverTime 交叉操作耗时
 *   │   ├── gaMutateTime    变异操作耗时
 *   │   ├── gaDecodeTime    二进制解码 w 耗时
 *   │   └── gaEvalTime      适应度评估总耗时（BP 调用累计）
 *   │       └── bpTime      分支定价总耗时
 *   │           ├── cgTime      列生成总耗时
 *   │           │   ├── pricingTime    定价子问题总耗时
 *   │           │   ├── cplexBuildTime RMP 模型构建耗时
 *   │           │   └── cplexSolveTime CPLEX 求解 RMP 耗时
 *   │           └── bbTime      分支定界总耗时
 *   ├── postTime      后处理耗时
 *   └── logTime       日志输出耗时（粗略估算）
 */
struct TimingReport {
    // 总时间
    double solveTime = 0.0;      // 总求解时间（GA 入口到返回）

    // GA 层 (Level 4)
    double gaTime = 0.0;         // GA 搜索总时间
    double gaSelectTime = 0.0;   // GA 选择操作
    double gaCrossoverTime = 0.0;// GA 交叉操作
    double gaMutateTime = 0.0;   // GA 变异操作
    double gaDecodeTime = 0.0;   // GA 二进制解码
    double gaEvalTime = 0.0;     // GA 适应度评估（BP 调用累计）

    // BP 层 (Level 3)
    double bpTime = 0.0;         // 分支定价总耗时（墙钟口径）
    double bpForkTime = 0.0;     // 并行管理：fork 调用耗时
    double bpPipeIoTime = 0.0;   // 并行管理：pipe/read/write I/O 耗时
    double bpWaitTime = 0.0;     // 并行管理：waitpid 等待耗时
    double bpMergeTime = 0.0;    // 并行管理：父进程聚合结果耗时
    double bpOtherTime = 0.0;    // BP 未归类耗时（残差）

    // CG 层 (Level 2)
    double cgTime = 0.0;         // 列生成总耗时
    double cplexBuildTime = 0.0; // CPLEX RMP 模型构建耗时
    double cplexSolveTime = 0.0; // CPLEX 求解 RMP 耗时

    // PS 层 (Level 1)
    double pricingTime = 0.0;    // 定价子问题总耗时

    // BB 层
    double bbTime = 0.0;         // 分支定界总耗时

    // 其他
    double postTime = 0.0;       // 后处理耗时
    double logTime = 0.0;        // 日志输出总耗时（估算）
};

/**
 * 单次实验的结果数据结构
 */
struct ExperimentResult {
    // 元信息
    std::string experimentName;
    int numDC = 0;
    int numRetailers = 0;
    double delta = 0.0;
    int randomSeed = 0;

    // 核心指标
    double totalProfit = 0.0;
    double carbonEmission = 0.0;
    double carbonCap = 0.0;     // 碳配额 C
    double ePlus = 0.0;         // 超额需购买的碳量 max(E-C, 0)
    double eMinus = 0.0;        // 富余可出售的碳量 max(C-E, 0)
    double solveTime = 0.0;
    int numDCsOpen = 0;
    int numRtsServed = 0;
    SolveStatus solveStatus = SolveStatus::NOT_STARTED;
    double bestBound = DBL_MAX;
    double relativeGap = DBL_MAX;
    bool hasIntegerSolution = false;

    // 算法统计
    int totalBranchNodes = 0;
    int prunedNodes = 0;
    int totalCGIterations = 0;
    int totalColumnsGenerated = 0;
    int totalGAGenerations = 0;

    // 时间分布
    TimingReport timing;

    // 完整报告文本（由 Monitor 生成，供终端/run.log/report.txt 统一使用）
    std::string timingReportText;
    std::string summaryText;

    // 最优解详情
    std::vector<double> bestW;
    std::vector<DCSolution> dcSolutions;
    // CarbonResult 由后处理填充

    // 收敛曲线
    std::vector<ConvergencePoint> convergence;
};

#endif // EXPERIMENT_RESULT_H
