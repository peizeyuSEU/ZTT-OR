#ifndef BRANCH_AND_PRICE_H
#define BRANCH_AND_PRICE_H

#include "../10_common/Instance.h"
#include "../10_common/Types.h"
#include "../10_common/Solution.h"
#include "../10_common/Config.h"
#include "../4_logger/Logger.h"
#include "../3_monitor/Monitor.h"
#include "../7_formula/06_CarbonCredit.h"
#include "../7_formula/05_InvestmentCost.h"
#include "../7_formula/04_InventoryCost.h"
#include "02_ColumnGeneration.h"
#include "03_BranchAndBound.h"
#include <vector>
#include <iostream>
#include <chrono>
#include <cmath>

/**
 * 分支定价封装层（Branch and Price）
 *
 * 对应论文第3层：列生成 + 分支定界 = 分支定价
 * 在固定 w_j 下精确求解整数规划问题。
 *
 * 日志层级：Level 1 [BP]，缩进2空格
 */
class BranchAndPrice {
private:
    const Instance* inst;
    const Config* config;
    Logger* logger;
    Monitor* monitor;
    ColumnGeneration columnGen;
    BranchAndBound bnb;
    double bpTime;

    // 跨 solve() 调用的时间累计器（每次真实求解后累加 bnb 本次统计）。
    // 因为 bnb.solve() 每次会 resetStats() 清零内部 CG/PS 计时，
    // 单纯转发 bnb.getCGTime() 只能反映最后一次调用，故在此层做全程累加。
    double accCGTime_;
    double accPricingTime_;
    double accBBTime_;
    double accCplexBuildTime_;
    double accCplexSolveTime_;

    int accCGIterations_;
    int accCGColumns_;
    int accTotalNodes_;
    int accPrunedNodes_;

    // ===== 细粒度诊断累加器（临时，定位 CG other 去向，全程累计） =====
    double accDbgDualTime_ = 0.0;
    double accDbgIntCheckTime_ = 0.0;
    double accDbgFillTime_ = 0.0;
    double accDbgAddColTime_ = 0.0;
    long long accDbgIntCheckCalls_ = 0;
    long long accDbgFillCalls_ = 0;

    // 缓存：对相同的w向量直接返回已计算的适应度
    struct CacheEntry {
        double profit;
        Solution sol;
    };
    std::unordered_map<std::string, CacheEntry> fitnessCache;

    // 最近一次 solve() 是否命中缓存（未触发真实求解），供串行累加口径判断是否计数。
    bool lastHitCache_ = false;

public:
    BranchAndPrice() : inst(nullptr), config(nullptr), logger(nullptr), monitor(nullptr), bpTime(0.0),
                       accCGTime_(0.0), accPricingTime_(0.0), accBBTime_(0.0),
                       accCplexBuildTime_(0.0), accCplexSolveTime_(0.0),
                       accCGIterations_(0), accCGColumns_(0),
                       accTotalNodes_(0), accPrunedNodes_(0) {}

    void setInstance(const Instance& instance) {
        inst = &instance;
        columnGen.setInstance(instance);
        bnb.setInstance(instance);
    }

    void setConfig(const Config& cfg) {
        config = &cfg;
        columnGen.setConfig(cfg);
        columnGen.setRCEps(cfg.rc_eps);
        bnb.setConfig(cfg);

        // ===== 环节配置横幅：BP 层（第3层，固定 w 下精确求解 IP，只打印一次） =====
        static bool bpBannerPrinted = false;
        if (!bpBannerPrinted) {
            std::cout << "\n  ========== [环节2/5] 分支定价 BP（固定 w_j 下精确求解整数规划） ==========" << std::endl;
            std::cout << "    设计: 列生成(求 LP 松弛) + 分支定界(恢复整数) = 分支定价" << std::endl;
            std::cout << "    收尾: LP 目标 + 碳配额收入 - 减排投资成本(½δw²√D) = 个体适应度" << std::endl;
            std::cout << "    优化: fitnessCache 以 w 向量为键去重，重复 w 直接返回历史利润(避免重算整棵搜索树)" << std::endl;
            std::cout << "    投资成本模型: " << (cfg.use_sqrt_investment ? "½δw²√D(论文标准)" : "½βw²") << std::endl;
            bpBannerPrinted = true;
        }
    }

    void setLogger(Logger* log) {
        logger = log;
        columnGen.setLogger(log);
        bnb.setLogger(log);
    }

     /** 透传：控制 CG 迭代进度是否写终端（子进程并行时应关掉，只写各自日志文件）。
     *  真正执行列生成的是 bnb 内部的 columnGen，故透传给 bnb。 */
    void setConsoleProgress(bool on) { bnb.setConsoleProgress(on); }

    void setMonitor(Monitor* mon) { monitor = mon; }

    double getBPTime() const { return bpTime; }
    double getCGTime() const { return accCGTime_; }
    double getPricingTime() const { return accPricingTime_; }
    double getBBTime() const { return accBBTime_; }
    double getCplexBuildTime() const { return accCplexBuildTime_; }
    double getCplexSolveTime() const { return accCplexSolveTime_; }
    int getCGIterations() const { return accCGIterations_; }
    int getCGTotalColumns() const { return accCGColumns_; }

    // 「本次求解」增量统计：bnb 每次 solve 开头会 resetStats() 清零其内部计数，
    // 故 bnb.getXxx() 恒为最近一次 solve() 的单次值。串行评估逐个体累加时用它，
    // 与并行 fork 子进程「每个个体一次求解」的口径完全对齐。
    // 注意：fitnessCache 命中时不会调用 bnb.solve()，这些值仍是上一次真实求解的残留，
    // 故调用方需在缓存未命中（真实求解）时才累加，见 solveOnce()。
    int getLastCGIterations() const { return bnb.getCGIterations(); }
    int getLastCGColumns() const { return bnb.getCGTotalColumns(); }
    int getLastTotalNodes() const { return bnb.getTotalNodes(); }
    int getLastPrunedNodes() const { return bnb.getPrunedNodes(); }
    // 本次 solve() 是否命中缓存（未做真实求解）。串行累加口径据此跳过缓存命中项。
    bool lastCallHitCache() const { return lastHitCache_; }

    /**
     * 对固定w求解分支定价
     *
     * @param w 减排率向量
     * @param label 个体标识（用于日志）
     * @return 最大利润（适应度）
     */
    double solve(const std::vector<double>& w, const std::string& label = "") {
        auto start = std::chrono::steady_clock::now();

        // 检查缓存
        std::string key = makeKey(w);
        auto cacheIt = fitnessCache.find(key);
        if (cacheIt != fitnessCache.end()) {
            lastHitCache_ = true;  // 命中缓存：本次未做真实求解，串行累加口径应跳过
            if (logger && !label.empty()) logger->log(1, label + "命中缓存");
            // 缓存命中不做新求解，上报当前累计时间让 monitor 保持一致
            if (monitor) {
                monitor->reportCGTiming(accCGTime_,
                                         accCplexBuildTime_,
                                         accCplexSolveTime_);
                monitor->reportPricingTiming(accPricingTime_);
                monitor->reportBBTiming(accBBTime_);
                monitor->reportBBNodes(accTotalNodes_, accPrunedNodes_);
                monitor->reportCGIteration(accCGIterations_, accCGColumns_);
            }
            return cacheIt->second.profit;
        }

        if (logger && !label.empty()) {
            logger->progress(1, label + "开始，w=[" + wToString(w) + "]");
        }

  // 设置减排率
        // const_cast 说明：inst 以 const Instance* 持有（求解器只读语义），
        // 但 w 是求解期需写入的可变状态。此处去 const 修改是安全的，前提：
        //   1) 串行求解：GA 逐个体顺序调用 solve()，单线程独占，无写竞争；
        //   2) 并行求解：GA fork 出的每个子进程持有 inst->deepCopy() 的独立副本
        //      （见 GeneticAlgorithm.h launchChild），子进程各自 setInstance，
        //      父进程与其它子进程不共享该 Instance，故无跨进程写竞争。
        // 【维护警告】若未来改为多线程共享同一 Instance 求解，必须移除此 const_cast
        // 并为每个线程提供独立 Instance 副本，否则 w 会被并发覆盖导致结果错乱。
        Instance& mutableInst = const_cast<Instance&>(*inst);
        mutableInst.setEmissionReductionRates(w);

        // 调用分支定界求解（w已设置，无需传参）
        double result = bnb.solve();
        lastHitCache_ = false;  // 走到这里表示做了真实求解，串行累加口径应计入本次增量

        // 加回碳配额收入
        if (result > -DBL_MAX / 2) {
            result += CarbonCreditFormula::carbonCredit(inst->p, inst->C);
            // 统一扣除减排投资成本（在列利润中不包含，在此统一处理）
            // 使用公式层计算：½ * δ * w_j² * √D_j（论文标准版）
            double investCost = 0.0;
            for (int j = 0; j < inst->numDC; j++) {
                double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
                // 计算D_j需要知道该DC服务的零售商标的，
                // 但投资成本是每个DC的固定成本，不依赖具体服务集合S，
                // 这里用分支定界最优解中该DC的D_j来算
                double D_j = 0.0;
                const Solution& sol = bnb.getBestSolution();
                for (const auto& dcSol : sol.dcSolutions) {
                    if (dcSol.dcIndex == j) {
                        for (int i = 0; i < inst->numRetailer; i++) {
                            if (dcSol.S[i] == 1) D_j += inst->mu[i];
                        }
                        break;
                    }
                }
                bool useSqrtModel = config ? config->use_sqrt_investment : true;
                investCost += InvestmentCostFormula::compute(*inst, j, w_j, D_j, useSqrtModel);
            }
            result -= investCost;
        }

        // 存入缓存
        CacheEntry entry;
        entry.profit = result;
        entry.sol = bnb.getBestSolution();
        // 以原始 w 的 key 写入，保证本次调用（及完全相同的 w）能命中。
        fitnessCache[key] = entry;

        // ===== 严谨性增强：写入「规范化 w」的等价 key =====
        // 未开启的 DC（服务集为空，D_j=0）其减排投资成本 ½δw_j²√D_j 恒为 0，
        // 故 w_j 取任何值对总利润无影响。GA 可能给这些 DC 配非零 w_j，
        // 使得本应等价的解产生不同 key，重复求解整棵搜索树、拉低缓存命中率。
        // 此处按最优解的开启模式把未开 DC 的 w_j 归零，得到 canonicalW，
        // 额外写入其 key，令后续开关模式相同的等价解可直接命中历史利润。
        {
            const Solution& bestSol = bnb.getBestSolution();
            std::vector<bool> dcOpened(inst->numDC, false);
            for (const auto& dcSol : bestSol.dcSolutions) {
                if (dcSol.dcIndex < 0 || dcSol.dcIndex >= inst->numDC) continue;
                for (int i = 0; i < inst->numRetailer; i++) {
                    if (dcSol.S[i] == 1) {
                        dcOpened[dcSol.dcIndex] = true;
                        break;
                    }
                }
            }
            std::vector<double> canonicalW = w;
            for (int j = 0; j < inst->numDC && j < (int)canonicalW.size(); j++) {
                if (!dcOpened[j]) canonicalW[j] = 0.0;  // 未开 DC 的 w_j 归零
            }
            std::string canonicalKey = makeKey(canonicalW);
            // 仅当规范 key 与原始 key 不同时才额外写入，避免冗余覆盖。
            if (canonicalKey != key) {
                fitnessCache[canonicalKey] = entry;
            }
        }

        auto end = std::chrono::steady_clock::now();
        bpTime += std::chrono::duration<double>(end - start).count();

        // 累加本次真实求解的细化时间（bnb 下次 solve 会清零其内部统计，故此处必须累加）
        accCGTime_ += bnb.getCGTime();
        accPricingTime_ += bnb.getPricingTime();
        accBBTime_ += bnb.getBBTime();
        accCplexBuildTime_ += bnb.getCplexBuildTime();
        accCplexSolveTime_ += bnb.getCplexSolveTime();
        accCGIterations_ += bnb.getCGIterations();
        accCGColumns_ += bnb.getCGTotalColumns();
        accTotalNodes_ += bnb.getTotalNodes();
        accPrunedNodes_ += bnb.getPrunedNodes();

        // ===== 细粒度诊断累加（定位 CG other 去向）=====
        // 默认关闭：这些累加与底层 chrono 计时只在排障时需要，生产路径不计开销。
        // 需再次诊断 CG other 时间去向时，编译加 -DCG_DIAG 即可启用累加与打印。
#ifdef CG_DIAG
        accDbgDualTime_ += bnb.getDbgDualTime();
        accDbgIntCheckTime_ += bnb.getDbgIntCheckTime();
        accDbgFillTime_ += bnb.getDbgFillTime();
        accDbgAddColTime_ += bnb.getDbgAddColTime();
        accDbgIntCheckCalls_ += bnb.getDbgIntCheckCalls();
        accDbgFillCalls_ += bnb.getDbgFillCalls();
        {
            double dbgSum = accDbgDualTime_ + accDbgIntCheckTime_
                          + accDbgFillTime_ + accDbgAddColTime_;
            std::cerr << "[CG-DBG累计] CG=" << accCGTime_ << "s"
                      << " | 对偶提取=" << accDbgDualTime_ << "s"
                      << " | 整数检查=" << accDbgIntCheckTime_ << "s"
                      << "(" << accDbgIntCheckCalls_ << "次getValue)"
                      << " | result填充=" << accDbgFillTime_ << "s"
                      << "(" << accDbgFillCalls_ << "次getValue)"
                      << " | 加列去重=" << accDbgAddColTime_ << "s"
                      << " | 四项合计=" << dbgSum << "s"
                      << " | 未覆盖余项=" << (accCGTime_ - accDbgDualTime_
                         - accDbgIntCheckTime_ - accDbgFillTime_
                         - accDbgAddColTime_ - accCplexBuildTime_
                         - accCplexSolveTime_ - accPricingTime_) << "s"
                      << std::endl;
        }
#endif

        // 上报累计细化时间到 monitor（跨所有 BP 调用累加，反映真实瓶颈分布）
        if (monitor) {
            monitor->reportCGTiming(accCGTime_,
                                     accCplexBuildTime_,
                                     accCplexSolveTime_);
            monitor->reportPricingTiming(accPricingTime_);
            monitor->reportBBTiming(accBBTime_);
            monitor->reportBBNodes(accTotalNodes_, accPrunedNodes_);
            monitor->reportCGIteration(accCGIterations_, accCGColumns_);
        }

        if (logger && !label.empty()) {
            logger->progress(1, label + "适应度 = " + std::to_string(result)
                + "（" + std::to_string(bnb.getTotalNodes()) + "节点，"
                + std::to_string(bnb.getCGIterations()) + " CG迭代）");
        }

        return result;
    }

    /** 获取最优解 */
    const Solution& getBestSolution() const { return bnb.getBestSolution(); }

    /** 获取分支定界统计 */
    int getTotalNodes() const { return accTotalNodes_; }
    int getPrunedNodes() const { return accPrunedNodes_; }
    int getIntegerSolutions() const { return bnb.getIntegerSolutions(); }

private:
    // 以 w 向量生成缓存键：将每个分量按 1e-4 精度离散化后拼接。
    // 用 std::lround 做四舍五入（而非 static_cast<int> 向零截断），
    // 避免 GA 解码产生的浮点值（如 0.34 存为 0.339999...）在整数边界附近
    // 被截断到相邻桶，导致等价的 w 生成不同 key、拉低缓存命中率。
    // 负值/越界值被 clamp 到 [0,1]，防御异常输入。
    static std::string makeKey(const std::vector<double>& w) {
        std::string key;
        for (double v : w) {
            double clamped = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
            key += std::to_string(std::lround(clamped * 10000)) + ",";
        }
        return key;
    }

    static std::string wToString(const std::vector<double>& w) {
        std::string s;
        for (size_t i = 0; i < w.size(); i++) {
            if (i > 0) s += ", ";
            s += std::to_string(w[i]);
        }
        return s;
    }
};

#endif // BRANCH_AND_PRICE_H
