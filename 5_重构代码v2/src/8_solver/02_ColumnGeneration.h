#ifndef COLUMN_GENERATION_H
#define COLUMN_GENERATION_H

#include "../10_common/Instance.h"
#include "../10_common/Types.h"
#include "../10_common/SolveDeadline.h"
#include "../10_common/Config.h"
#include "../7_formula/08_Objective.h"
#include "CplexWrapper.h"
#include "../4_logger/Logger.h"
#include "../10_common/ThreadPool.h"
#include "01_PricingSolver.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <chrono>
#include <memory>
#include <thread>
#include <algorithm>

/**
 * 列生成求解器（Column Generation）
 *
 * 采用与原代码 GA_BP.cpp 一致的方式：
 * 在同一个 CPLEX 环境中动态添加新列，无需每轮重建模型。
 */
class ColumnGeneration {
private:
    const Instance* inst;
    const Config* config = nullptr;
    // 每个DC一个持久定价求解器：缓存(cachedBestCols)与计时跨迭代保留；
    // 并行时线程 j 独占 pricingSolvers_[j]（按DC下标天然分区，各元素互不访问，无需加锁）。
    std::vector<PricingSolver> pricingSolvers_;
    // 持久线程池：仅在 setInstance 时创建一次，全程复用，避免每轮 CG 迭代
    // 反复 new/join 4 个线程（48399 轮 → 约 39 万次线程建销的巨额开销）。
    std::unique_ptr<ThreadPool> pricingPool_;
    // 定价线程池实际线程数（按核预算算得，供横幅打印与调试）。
    int pricingThreads_ = 0;
    // 并行定价的墙钟计时累加器：并行时各 DC 计时重叠，求和会超过墙钟，
    // 故单独按"整批并行段"的墙钟时间累计，作为 PS 的真实耗时口径。
    double parallelPricingWallTime_ = 0.0;
    ObjectiveFormula objFormula;
    Logger* logger;
    double rc_eps;
    double cgTime;
    double cplexBuildTime;
    double cplexSolveTime;
    int totalIterations;
    int totalColumns;
    bool verbose_ = false;      // 调试模式：输出更多细节到文件
    int lastIterPricingCalls_ = 0;  // 上一轮迭代中调定价的次数（去重后）
    bool consoleProgress_ = true;  // true=用progress输出到终端, false=用log只写文件

    // ===== 细粒度诊断累加器（临时，定位 CG other 去向，跨所有节点累计） =====
    double dbgDualTimeTotal_ = 0.0;
    double dbgIntCheckTimeTotal_ = 0.0;
    double dbgFillTimeTotal_ = 0.0;
    double dbgAddColTimeTotal_ = 0.0;
    long long dbgIntCheckCallsTotal_ = 0;
    long long dbgFillCallsTotal_ = 0;

    // ===== 方案A：对偶价格平滑（Wentges smoothing，消除对偶震荡 cycling） =====
    // 单节点 CG 内部持久保留上一轮"喂给定价"的平滑对偶，作为稳定参考点 π̄。
    // 每轮实际喂给定价的价格 = alpha * π̄_prev + (1 - alpha) * π_本轮LP对偶。
    // 只影响列的生成路径，不改变 LP 最优值（收敛判据仍严格用 RC>rc_eps）。
    std::vector<double> smoothedDual_;    // 上一轮平滑后的对偶（π̄）
    bool hasSmoothedDual_ = false;        // 单节点首轮无历史，直接用原始对偶
    // ===== 自适应稳定化（消除退化算例 w=0 上的 CG cycling 死循环） =====
    // 运行时生效的平滑强度 α（整棵 BB 树开始时在 resetStats() 初始化为 config 值，
    // 之后跨节点粘滞保留衰减结果，不在每个 solve 节点重置）。
    // 触发 mis-pricing（平滑对偶假收敛、真实对偶下仍有改进列）时的响应策略：
    //
    // 关键决策——【一步归零，而非渐进砍半】：
    //   实测发现渐进砍半（0.5→0.25→…）存在致命副作用：α 归零前的每一轮仍用
    //   带平滑的错向对偶定价，把大量次优列灌进【永久列池】。等 α 衰减到 0，
    //   列池已被污染，根节点 LP 被拖低到分数解 → 触发 BB 分支爆炸（w=0 上实测
    //   BB 节点数暴涨到数百，对照 smoothOFF 仅 56 轮/BB 0 节点/0.69s）。
    //   mis-pricing 的语义是明确的——"平滑此刻在给假收敛信号"，最安全的响应就是
    //   【立即彻底关闭平滑】：α 一步归零并清空平滑历史，本节点起改用真实对偶，
    //   不再向列池灌入任何错向次优列。配合上面的跨节点粘滞，退化算例一旦被某节点
    //   识破，整棵 BB 树后续都等价于纯 smoothOFF 行为（干净收敛、根节点整数解）。
    //   · 健康算例几乎不触发 mis-pricing → α 维持初值，全程享受平滑加速；
    //   · 即便健康算例偶发一次误触发，代价也仅是退回无平滑的健康行为（无害）；
    //   · 退化算例首次识破即归零 → 一套逻辑通吃所有算例，无需按 w 特判。
    double currentSmoothAlpha_ = 0.0;               // 当前生效的平滑 α（运行时可变）
    // 砍半系数设为 0.0：mis-pricing 触发时 α *= 0.0 一步归零（见上"一步归零"决策）。
    static constexpr double kSmoothAlphaShrink = 0.0;
    static constexpr double kSmoothAlphaFloor = 0.05;   // α 降到此阈值以下即完全关闭平滑
    // 平滑震荡检测阈值：平滑开启下连续这么多轮 obj 未创新高即判定平滑有害并关闭。
    // 取值需大于健康算例正常的暂时性平台期（几轮），又要能及时截停退化算例的长期震荡。
    static constexpr int kSmoothStallLimit = 20;

public:
    ColumnGeneration() : inst(nullptr), logger(nullptr), rc_eps(1e-6), cgTime(0.0),
                         cplexBuildTime(0.0), cplexSolveTime(0.0),
                         totalIterations(0), totalColumns(0) {}

    void setInstance(const Instance& instance) {
        inst = &instance;
        // 按DC数量分配持久求解器（一次性分配，迭代中不再 resize，故并行访问安全）
        int numDC = static_cast<int>(instance.getNumDC());
        pricingSolvers_.resize(numDC);
        for (auto& ps : pricingSolvers_) ps.setInstance(instance);
        // 注意：定价线程池在 setConfig() 中按核预算创建（此处 config 尚不可用）。
    }

    void setConfig(const Config& cfg) {
        config = &cfg;
        objFormula.setConfig(cfg);
        for (auto& ps : pricingSolvers_) ps.setConfig(cfg);

        // ===== 定价线程池：按"总核预算"约束下的线程数创建（幂等，可重复调用） =====
        // 两层并行的核占用模型：GA 外层 fork 出 gaProcs 个子进程（个体级并行），
        // 每个子进程内定价线程池再开 pThreads 个线程 → 峰值占核 = gaProcs × pThreads。
        // core_budget 就是给这个乘积设的上限，避免 GA 并行时吃光服务器所有核。
        int numDC = static_cast<int>(pricingSolvers_.size());
        // GA 并发进程数：仅当个体级并行开启且线程数>1 时才真正 fork 多进程，否则串行=1。
        int gaProcs = 1;
        if (cfg.parallel_fitness) {
            int nt = cfg.num_threads;
            if (nt <= 0) {
                nt = static_cast<int>(std::thread::hardware_concurrency());
                if (nt <= 0) nt = 1;
            }
            if (nt > 1) gaProcs = nt;
        }
        // 定价线程数 = min(DC 数, 定价线程上限, 预算/GA进程数)，下限 1。
        // core_budget=0 表示不设预算 → 严格退化为现状 min(numDC, pricing_threads)。
        int upper = std::max(1, cfg.pricing_threads);
        int pThreads = std::min(numDC, upper);
        if (cfg.core_budget > 0) {
            int perProc = std::max(1, cfg.core_budget / std::max(1, gaProcs));
            pThreads = std::min(pThreads, perProc);
        }
        pThreads = std::max(1, pThreads);
        pricingThreads_ = pThreads;  // 始终记录设定线程数(供横幅展示)，与是否真正建池解耦。
        // 仅在"确实要并行"且多 DC、线程数>1 时才建池；串行(parallel_pricing=false)不浪费建池。
        // 线程数变化时重建，否则复用（幂等防重复建池）。
        bool needPool = cfg.parallel_pricing && numDC > 1 && pThreads > 1;
        if (needPool) {
            if (!pricingPool_ ||
                static_cast<int>(pricingPool_->size()) != pThreads) {
                pricingPool_ = std::make_unique<ThreadPool>(pThreads);
            }
        } else {
            pricingPool_.reset();
        }

        // ===== 环节配置横幅：CG + PS 层（第1层，最内核，只打印一次） =====
        static bool cgBannerPrinted = false;
        if (!cgBannerPrinted) {
            bool parPricing = cfg.parallel_pricing && numDC > 1;
            std::cout << "\n      ========== [环节4/5] 列生成 CG（RMP + 定价子问题迭代求 LP 松弛） ==========" << std::endl;
            std::cout << "        设计: CPLEX 增量式 RMP(动态加列，不重建模型) ↔ 逐 DC 定价生成负 RC 新列" << std::endl;
            // 定价并行配置：无论是否启用，都完整展示线程/预算设定（未启用时仅不生效，便于核对配置）。
            std::cout << "        定价并行: " << (parPricing ? "是(持久线程池复用)" : "否(串行逐DC)")
                      << " | DC 数=" << numDC
                      << " | 定价线程=" << pricingThreads_
                      << "(上限pricing_threads=" << cfg.pricing_threads << ")"
                      << (parPricing ? "[生效]" : "[未启用,parallel_pricing=false]")
                      << std::endl;
            std::cout << "        核预算: core_budget=" << cfg.core_budget
                      << (cfg.core_budget > 0 ? "" : "(0=不限制)")
                      << " | GA并发进程数=" << (cfg.parallel_fitness ? "见num_threads" : "1(GA串行)")
                      << " | 峰值占核=GA进程×定价线程" << std::endl;
            // RMP(CPLEX) 线程数：GA fork 并行时上层已把 cplex_threads 强制回退为 1（防过度订阅），
            // 故此处如实展示当前 config 生效值，便于核对是否放开多线程加速 LP 求解。
            {
                int rmpThreads = (config && config->cplex_threads > 0) ? config->cplex_threads : 1;
                std::cout << "        RMP求解: CPLEX线程=" << rmpThreads
                          << "(cplex_threads=" << cfg.cplex_threads << ")"
                          << (cfg.parallel_fitness
                                  ? "[GA并行时子进程强制回退1避免过度订阅]"
                                  : (rmpThreads > 1 ? "[多线程加速LP]" : "[单线程]"))
                          << std::endl;
            }
            std::cout << "        收敛: 最大迭代=" << cfg.max_cg_iterations
                      << " | tailing-off 早停=" << (cfg.cg_early_stop ? "是" : "否")
                      << "(连续" << cfg.cg_stall_iterations << "轮改善<" << cfg.cg_stall_tolerance << ")"
                      << " | RC 阈值=" << cfg.rc_eps << std::endl;
            std::cout << "        优化: 每DC持久 PricingSolver(缓存跨迭代保留) + 持久线程池(不再每轮建销)" << std::endl;
            std::cout << "\n      ========== [环节5/5] 定价子问题 PS（每DC生成最大负 RC 列，本次瓶颈~83%） ==========" << std::endl;
            const char* pricingLabel =
                (cfg.pricing_algorithm == 1)
                    ? "论文Algorithm3（正式校验模式）"
                    : ((cfg.pricing_algorithm == 2)
                           ? "ConvexHull/Simplified候选 + Algorithm3完整校验"
                           : "Simplified候选 + Algorithm3完整校验");
            std::cout << "        算法: " << pricingLabel << std::endl;
            std::cout << "        无损缓存: cachedBestCols仅提供 incumbent/上界剪枝，不跳过完整定价" << std::endl;
            std::cout << "        完整定价: 候选价格(保留价去重)×两点组合 O(候选价×n_neg²×numRetailer)" << std::endl;
            cgBannerPrinted = true;
        }
    }

    void setRCEps(double eps) {
        rc_eps = eps;
        for (auto& ps : pricingSolvers_) ps.setRCEps(eps);
    }

    void setLogger(Logger* log) {
        logger = log;
        for (auto& ps : pricingSolvers_) ps.setLogger(log);
    }

    /** 设置是否输出CG迭代进度到终端（根节点=true，分支节点=false） */
    void setConsoleProgress(bool on) { consoleProgress_ = on; }

    double getCGTime() const { return cgTime; }
    double getPricingTime() const {
        // 并行定价：各 DC 计时重叠，求和会超过墙钟，改用整批并行段的墙钟累计。
        if (parallelPricingWallTime_ > 0.0) return parallelPricingWallTime_;
        // 串行定价：各 solver 计时不重叠，求和即真实耗时。
        double t = 0.0;
        for (const auto& ps : pricingSolvers_) t += ps.getPricingTime();
        return t;
    }
    double getCplexBuildTime() const { return cplexBuildTime; }
    double getCplexSolveTime() const { return cplexSolveTime; }
    int getIterations() const { return totalIterations; }
    int getTotalColumns() const { return totalColumns; }

    // ===== 细粒度诊断 getter（临时） =====
    double getDbgDualTime() const { return dbgDualTimeTotal_; }
    double getDbgIntCheckTime() const { return dbgIntCheckTimeTotal_; }
    double getDbgFillTime() const { return dbgFillTimeTotal_; }
    double getDbgAddColTime() const { return dbgAddColTimeTotal_; }
    long long getDbgIntCheckCalls() const { return dbgIntCheckCallsTotal_; }
    long long getDbgFillCalls() const { return dbgFillCallsTotal_; }

    /** 重置统计累加器（在多次BP调用间调用） */
    void resetStats() {
        cgTime = 0.0;
        cplexBuildTime = 0.0;
        cplexSolveTime = 0.0;
        totalIterations = 0;
        totalColumns = 0;
        dbgDualTimeTotal_ = 0.0;
        dbgIntCheckTimeTotal_ = 0.0;
        dbgFillTimeTotal_ = 0.0;
        dbgAddColTimeTotal_ = 0.0;
        dbgIntCheckCallsTotal_ = 0;
        dbgFillCallsTotal_ = 0;
        parallelPricingWallTime_ = 0.0;
        for (auto& ps : pricingSolvers_) ps.resetStats();
        // 自适应稳定化：整棵 BB 树开始时把生效 α 初始化为配置初值。
        // 关键：α 的衰减在整棵树内“粘滞保留”（见 solve 入口不再重置），
        // 一旦某节点识别出算例退化把 α 砍下来，后续所有节点沿用降后的 α，
        // 避免每个子节点都从 0.5 重新震荡→降级，造成 BB 节点暴涨。
        currentSmoothAlpha_ = (config && config->dual_smooth_alpha > 0.0)
                                  ? config->dual_smooth_alpha
                                 : 0.0;
    }

    /**
     * 列生成求解。
     *
     * @param branchState 当前节点的分支约束
     * @param parentPool  方案C：父节点的列池（j_S/p_j）。非空时子节点以过滤后的
     *                    父列作为暖启动初始列池，跳过从 numDC*(numRetailer+1) 空集/单列
     *                    重建的重复爬坡；nullptr 时退化为冷启动（根节点用）。
     */
    CGResult solve(const BranchState& branchState,
                   const CGResult* parentPool = nullptr,
                   bool forceStrict = false,
                   const SolveDeadline* deadline = nullptr) {
        auto start = std::chrono::steady_clock::now();
        CGResult result;
        result.feasible = false;
        const int nodeStartIterations = totalIterations;
        const int nodeStartColumns = totalColumns;
        const double nodeStartPricingTime = getPricingTime();
        const double nodeStartCplexSolveTime = cplexSolveTime;
        long long nodePositiveCandidates = 0;
        long long nodeNewColumns = 0;
        long long nodeDuplicateCandidates = 0;
        long long nodeUpdatedColumns = 0;
        int nodeZeroImproveIterations = 0;
        double nodeFirstObjective = 0.0;
        double nodeLastObjective = 0.0;
        double nodePreviousObjective = 0.0;
        double nodeMaxReducedCost = -DBL_MAX;
        bool nodeHasObjective = false;
        auto logNodeDiagnostic = [&](const char* exitReason) {
            if (!logger) return;
            const double nodeWall = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();
            const double objectiveGain = nodeHasObjective
                                             ? nodeLastObjective - nodeFirstObjective
                                             : 0.0;
            logger->log(2, "[CG-NODE] depth="
                + std::to_string(branchState.retailerBranches.size())
                + " exit=" + std::string(exitReason)
                + " certified=" + std::to_string(result.certifiedOptimal ? 1 : 0)
                + " feasible=" + std::to_string(result.feasible ? 1 : 0)
                + " iters=" + std::to_string(totalIterations - nodeStartIterations)
                + " generatedColsBeforeNode=" + std::to_string(nodeStartColumns)
                + " newCols=" + std::to_string(nodeNewColumns)
                + " posCandidates=" + std::to_string(nodePositiveCandidates)
                + " duplicateCandidates=" + std::to_string(nodeDuplicateCandidates)
                + " updatedCols=" + std::to_string(nodeUpdatedColumns)
                + " zeroObjImproveIters=" + std::to_string(nodeZeroImproveIterations)
                + " objFirst=" + std::to_string(nodeFirstObjective)
                + " objLast=" + std::to_string(nodeLastObjective)
                + " objGain=" + std::to_string(objectiveGain)
                + " maxRC="
                + (nodeMaxReducedCost > -DBL_MAX / 2
                       ? std::to_string(nodeMaxReducedCost)
                       : std::string("N/A"))
                + " wallSec=" + std::to_string(nodeWall)
                + " pricingSec=" + std::to_string(getPricingTime() - nodeStartPricingTime)
                + " cplexSolveSec="
                + std::to_string(cplexSolveTime - nodeStartCplexSolveTime));
        };
        auto finishTimeLimit = [&]() -> CGResult {
            result.certifiedOptimal = false;
            result.timeLimitReached = true;
            const auto end = std::chrono::steady_clock::now();
            cgTime += std::chrono::duration<double>(end - start).count();
            logNodeDiagnostic("TIME_LIMIT");
            return result;
        };
        if (deadline && deadline->expired()) {
            return finishTimeLimit();
        }

        // 方案A：每次进入新节点(新 LP)时重置对偶平滑历史，避免跨节点串味。
        hasSmoothedDual_ = false;
        smoothedDual_.assign(inst->numDC + inst->numRetailer, 0.0);
        // 注意：currentSmoothAlpha_ 不在此重置——它在整棵 BB 树内粘滞保留衰减结果
        // （初始化在 resetStats()）。退化算例被识别后整棵树沿用降后的 α，避免每节点重复震荡。

        // ===== 细粒度诊断计时（临时，用于定位 CG other 去向） =====
        // 直接累加到成员累加器（dbg*Total_），确保任意 return 点退出时都已记账。

        CplexEnv env;

        try {
            int numDC = inst->numDC;
            int numRetailer = inst->numRetailer;

            // ===== 1. 初始化列集合 =====
            std::vector<std::vector<std::vector<int>>> j_S(numDC);
            std::vector<std::vector<double>> p_j(numDC);
            std::vector<std::vector<double>> profits(numDC);

            for (int j = 0; j < numDC; j++) {
                // 空集列
                std::vector<int> emptyS(numRetailer, 0);
                if (branchState.isColumnFeasible(j, emptyS)) {
                    // profit 口径须与定价加列(02:630 用 inst->w[j])一致，否则 RMP 目标
                    // 系数为 w=0 的旧值、定价用真实 w 重算，导致同 S 列被反复误报有改进
                    // (rc 虚高) 又被去重挡掉，列生成永不收敛。
                    double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
                    double emptyProfit = objFormula.columnProfit(*inst, j, emptyS, 0.0, w_j);
                    j_S[j].push_back(emptyS);
                    p_j[j].push_back(0.0);
                    profits[j].push_back(emptyProfit);
                }

                // 每个零售商单独服务的列
                for (int i = 0; i < numRetailer; i++) {
                    std::vector<int> S(numRetailer, 0);
                    S[i] = 1;
                    if (!branchState.isColumnFeasible(j, S)) continue;
                    double price = inst->reservePrice[i];
                    double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
                    double profit = objFormula.columnProfit(*inst, j, S, price, w_j);
                    j_S[j].push_back(S);
                    p_j[j].push_back(price);
                    profits[j].push_back(profit);
                }

                // 若该 DC 被强制服务某些零售商（x_ij=1），初始列可能全被过滤，
                // 补一个满足所有强制约束的初始列，保证 RMP 从可行列池起步。
                if (j_S[j].empty()) {
                    std::vector<int> forcedS(numRetailer, 0);
                    double price = 0.0;
                    bool hasForcedRetailer = false;
                    double forcedPriceCap = IloInfinity;
                    for (const auto& rb : branchState.retailerBranches) {
                        if (rb.dcIndex == j && rb.value == 1
                            && rb.retailerIndex < numRetailer) {
                            forcedS[rb.retailerIndex] = 1;
                            hasForcedRetailer = true;
                            forcedPriceCap = std::min(
                                forcedPriceCap,
                                inst->reservePrice[rb.retailerIndex]);
                        }
                    }
                    // 所有被强制服务的零售商都必须接受统一价格，故价格上限是
                    // 它们保留价的最小值，而不是最大值。
                    if (hasForcedRetailer) price = forcedPriceCap;
                    double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
                    double profit = objFormula.columnProfit(*inst, j, forcedS, price, w_j);
                    j_S[j].push_back(forcedS);
                    p_j[j].push_back(price);
                    profits[j].push_back(profit);
                }

                // ===== 方案C：注入父节点列池（暖启动） =====
                // 继承父节点已生成的列，避免子节点从空集/单列 6040 列重复爬坡。
                // 必须用当前分支约束过滤，剔除违反新增分支的父列；再对已有基座列去重。
                if (parentPool && j < (int)parentPool->j_S.size()) {
                    const auto& parentS = parentPool->j_S[j];
                    const auto& parentP = parentPool->p_j[j];
                    for (int m = 0; m < (int)parentS.size(); m++) {
                        const std::vector<int>& S = parentS[m];
                        if (!branchState.isColumnFeasible(j, S)) continue;
                        double pj = (m < (int)parentP.size()) ? parentP[m] : 0.0;
                        if (!RevenueFormula::serviceSetAcceptsPrice(
                                *inst, S, pj)) continue;
                        bool duplicated = false;
                        for (const auto& existingS : j_S[j]) {
                            if (existingS == S) {
                                duplicated = true;
                                break;
                            }
                        }
                        if (duplicated) continue;
                        double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
                        double profit = objFormula.columnProfit(*inst, j, S, pj, w_j);
                        j_S[j].push_back(S);
                        p_j[j].push_back(pj);
                        profits[j].push_back(profit);
                    }
                }
            }

            // ===== 2. 构建RMP模型（一次性构建，后续动态加列）=====
            auto buildStart = std::chrono::steady_clock::now();
            IloEnv envRaw = env.get();
            IloModel model(envRaw);
            IloObjective totalProfit = IloAdd(model, IloMaximize(envRaw));

            int numConstraints = numRetailer + numDC;
            IloNumArray low(envRaw, numConstraints);
            IloNumArray up(envRaw, numConstraints);
            for (int k = 0; k < numConstraints; k++) {
                low[k] = -IloInfinity;
                up[k] = 1;
            }
            // x_ij=1 分支不能只过滤掉 S[i]=0 的列；DC 约束原本是 <=1，
            // 仍允许一列都不选。只要某个 retailer 被该 DC 强制服务，就把
            // 该 DC 的列和约束改成 =1；结合列过滤后才真正等价于 x_ij=1。
            for (const auto& rb : branchState.retailerBranches) {
                if (rb.value == 1 && rb.dcIndex >= 0 && rb.dcIndex < numDC) {
                    low[numRetailer + rb.dcIndex] = 1.0;
                }
            }
            IloRangeArray Fill = IloAdd(model, IloRangeArray(envRaw, low, up));

            IloArray<IloNumVarArray> z_j(envRaw);

            for (int j = 0; j < numDC; j++) {
                IloNumVarArray z_j_s(env.get());
                for (int m = 0; m < (int)profits[j].size(); m++) {
                    IloNumColumn col = totalProfit(profits[j][m]);
                    for (int n = 0; n < numRetailer; n++) {
                        if (j_S[j][m][n] == 1) {
                            col = col + Fill[n](1);
                        }
                    }
                    int kk = numRetailer + j;
                    col = col + Fill[kk](1);
                    // 单列上界由 DC 约束 Σ_S z_{j,S}≤1 隐含保证。
                    // 不再额外设置 z≤1，避免定价 RC 漏掉变量上界对偶。
                    z_j_s.add(IloNumVar(col, 0, IloInfinity, ILOFLOAT));
                }
                z_j.add(z_j_s);
            }

            // 真实初始列数（暖启动后含继承的父列，非固定 numDC*(numRetailer+1)）
            int initialColumnCount = 0;
            for (int j = 0; j < numDC; j++) {
                initialColumnCount += (int)profits[j].size();
            }

            // 分支约束已在初始列过滤 + 定价子问题过滤中施加，
            // 无需在此对列位置索引设置边界（旧机制因列池重建而失效，已移除）。

            // ===== 3. 创建CPLEX求解器（只创建一次）=====
            IloCplex cplex(model);
            cplex.setOut(envRaw.getNullStream());
            cplex.setWarning(envRaw.getNullStream());
            // RMP 线程数由 config 控制（默认 1）。GA fork 并行时上层已强制回退为
            // 1，避免 fork 进程数 × CPLEX 线程数 过度订阅导致死锁。
            int cplexThreads = (config && config->cplex_threads > 0)
                                   ? config->cplex_threads
                                   : 1;
            cplex.setParam(IloCplex::Threads, cplexThreads);
            cplex.setParam(IloCplex::EpOpt, 1e-9);
            auto buildEnd = std::chrono::steady_clock::now();
            cplexBuildTime += std::chrono::duration<double>(buildEnd - buildStart).count();

            if (logger) {
                if (consoleProgress_)
                    logger->progress(2, "列生成开始，初始列数=" + std::to_string(initialColumnCount));
                else
                    logger->log(2, "列生成开始，初始列数=" + std::to_string(initialColumnCount));
            }

            // ===== 4. 列生成迭代 =====
            IloNumArray price(envRaw, numConstraints);

            // 迭代上限保护：防止定价子问题数值抖动导致的死循环
           int maxCgIter = (config && config->max_cg_iterations > 0)
                                ? config->max_cg_iterations
                                : 2000;

            // tailing-off 早停跟踪：记录上一轮 RMP 目标值与连续改善不足的轮数
            // forceStrict 时禁用 tailing-off 早停，强制 CG 走严格收敛出口
            // （真收敛 positiveCols.empty 或列池收敛 effectiveImproveCols==0），
            // 使返回的 lpObjective 成为严格 LP 上界。根节点调用时置 true，
            // 避免根 LP 被早停低估导致后续子节点 LP 顶穿、gap 变负。
            bool cgEarlyStop = forceStrict ? false : (config ? config->cg_early_stop : true);
            int stallLimit = (config && config->cg_stall_iterations > 0)
                                 ? config->cg_stall_iterations
                                 : 20;
            double stallTol = (config && config->cg_stall_tolerance > 0)
                                  ? config->cg_stall_tolerance
                                  : 1.0e-6;
            double prevObj = 0.0;
            bool hasPrevObj = false;
            int stallCount = 0;
            bool adaptiveMultiActive = false;

            for (int iter = 0; iter < maxCgIter; iter++) {
                if (deadline && deadline->expired()) {
                    return finishTimeLimit();
                }
                totalIterations++;

                // 4a. 求解RMP（计时）
                if (deadline && deadline->enabled()) {
                    const double remaining = deadline->remainingSeconds();
                    if (remaining <= 0.0) return finishTimeLimit();
                    cplex.setParam(IloCplex::TiLim,
                                   std::max(0.001, remaining));
                }
                auto cplexStart = std::chrono::steady_clock::now();
                bool hasSolution = cplex.solve();
                auto cplexEnd = std::chrono::steady_clock::now();
                cplexSolveTime += std::chrono::duration<double>(cplexEnd - cplexStart).count();

                if (deadline && deadline->expired()) {
                    return finishTimeLimit();
                }

                if (!hasSolution) {
                    if (logger) {
                        if (consoleProgress_)
                            logger->progress(2, "RMP求解失败（不可行）");
                        else
                            logger->log(2, "RMP求解失败（不可行）");
                    }
                    result.feasible = false;
                    auto end = std::chrono::steady_clock::now();
                    cgTime += std::chrono::duration<double>(end - start).count();
                    logNodeDiagnostic("RMP_INFEASIBLE");
                    return result;
                }

                double objValue = cplex.getObjValue();
                if (!nodeHasObjective) {
                    nodeFirstObjective = objValue;
                    nodePreviousObjective = objValue;
                    nodeHasObjective = true;
                } else {
                    const double improve = objValue - nodePreviousObjective;
                    const double scale = std::max(1.0, std::abs(nodePreviousObjective));
                    if (std::abs(improve) <= 1.0e-9 * scale) {
                        nodeZeroImproveIterations++;
                    }
                    nodePreviousObjective = objValue;
                }
                nodeLastObjective = objValue;

                // 4b. tailing-off 早停：连续多轮目标值几乎不再改善则提前收敛。
                //     列生成尾部常反复加入 RC 微正列，目标值实质不变，
                //     仅靠 rc_eps 过严会导致上万轮无效迭代，此处按相对改善量兜底。
                if (hasPrevObj) {
                    double denom = std::max(1.0, std::abs(prevObj));
                    double relImprove = std::abs(objValue - prevObj) / denom;
                    if (relImprove < stallTol) stallCount++;
                    else stallCount = 0;
                }
                prevObj = objValue;
                hasPrevObj = true;

                // 4c. 检查整数性
                bool isInteger = true;
#ifdef CG_DIAG
                auto intCheckStart = std::chrono::steady_clock::now();
#endif
                for (int j = 0; j < z_j.getSize() && isInteger; j++) {
                    for (int k = 0; k < z_j[j].getSize() && isInteger; k++) {
                        double val = cplex.getValue(z_j[j][k]);
#ifdef CG_DIAG
                        dbgIntCheckCallsTotal_++;
#endif
                        if (!CplexEnv::isIntegerValue(val, rc_eps)) {
                            isInteger = false;
                        }
                    }
                }
#ifdef CG_DIAG
                dbgIntCheckTimeTotal_ += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - intCheckStart).count();
#endif

                result.lpObjective = objValue;
                result.isInteger = isInteger;
                result.feasible = true;
                result.final_z_j.resize(z_j.getSize());
#ifdef CG_DIAG
                auto fillStart = std::chrono::steady_clock::now();
#endif
                for (int j = 0; j < z_j.getSize(); j++) {
                    result.final_z_j[j].resize(z_j[j].getSize());
                    for (int k = 0; k < z_j[j].getSize(); k++) {
                        result.final_z_j[j][k] = cplex.getValue(z_j[j][k]);
#ifdef CG_DIAG
                        dbgFillCallsTotal_++;
#endif
                    }
                }
#ifdef CG_DIAG
                dbgFillTimeTotal_ += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - fillStart).count();
#endif
                result.j_S = j_S;
                result.p_j = p_j;
                result.numColumns = totalColumns;

                // 4c. 获取对偶变量（即使整数解也需要，用于检查定价子问题是否还有正检验数列）
#ifdef CG_DIAG
                auto dualStart = std::chrono::steady_clock::now();
#endif
                for (int i = 0; i < numConstraints; i++) {
                    price[i] = cplex.getDual(Fill[i]);
                }
#ifdef CG_DIAG
                dbgDualTimeTotal_ += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - dualStart).count();
                // [DIAG-DUAL] 打印每轮所有对偶变量，用于分析冻结/退化
                {
                    std::string dualMsg = "[DIAG-DUAL] iter=" + std::to_string(totalIterations);
                    // 前 numRetailer 个是 π_i (零售商约束对偶)，后 numDC 个是 σ_j (DC容量约束对偶)
                    for (int d = 0; d < numConstraints; d++) {
                        if (d < numRetailer) {
                            dualMsg += " pi_" + std::to_string(d) + "="
                                + std::to_string(price[d]);
                        } else {
                            dualMsg += " sigma_" + std::to_string(d - numRetailer) + "="
                                + std::to_string(price[d]);
                        }
                    }
                    if (logger) logger->log(2, dualMsg);
                }
                // [DIAG-DEGEN] 退化诊断：统计基变量中取值≈0的数量（退化维度）
                {
                    int degenerateBasicCount = 0;
                    int totalBasicCount = 0;
                    for (int j = 0; j < z_j.getSize(); j++) {
                        for (int k = 0; k < z_j[j].getSize(); k++) {
                            IloCplex::BasisStatus s = cplex.getBasisStatus(z_j[j][k]);
                            if (s == IloCplex::Basic) {
                                totalBasicCount++;
                                double val = cplex.getValue(z_j[j][k]);
                                if (std::abs(val) < 1e-6) {
                                    degenerateBasicCount++;
                                }
                            }
                        }
                    }
                    if (logger) logger->log(2, "[DIAG-DEGEN] iter=" + std::to_string(totalIterations)
                        + " basicVars=" + std::to_string(totalBasicCount)
                        + " degenBasic=" + std::to_string(degenerateBasicCount)
                        + " degenDim=" + std::to_string(degenerateBasicCount));
                }
#endif

                // 4d. 调用定价子问题
                if (logger && verbose_) logger->log(2, "调用定价子问题");

                std::vector<double> dualVec(numConstraints);
                for (int i = 0; i < numConstraints; i++) {
                    dualVec[i] = price[i];
                }

                // ===== 方案A：对偶价格平滑（Wentges）消除对偶震荡 =====
                // π_喂给定价 = alpha * π̄_prev + (1 - alpha) * π_本轮LP对偶。
                // 首轮无历史则直接用原始对偶；alpha<=0 时退化为关闭平滑。
                // 注意：这里读运行时可变的 currentSmoothAlpha_ 而非直接读 config，
                //       它会在 mis-pricing 触发时被自适应砍半，直至降到阈值以下归 0。
                double smoothAlpha = (currentSmoothAlpha_ > 0.0)
                                         ? currentSmoothAlpha_
                                         : 0.0;
                if (smoothAlpha > 0.0 && hasSmoothedDual_) {
                    for (int i = 0; i < numConstraints; i++) {
                        dualVec[i] = smoothAlpha * smoothedDual_[i]
                                     + (1.0 - smoothAlpha) * price[i];
                    }
                }
                // 记录本轮"喂给定价"的对偶作为下一轮的稳定参考点 π̄。
                if (smoothAlpha > 0.0) {
                    for (int i = 0; i < numConstraints; i++) {
                        smoothedDual_[i] = dualVec[i];
                    }
                    hasSmoothedDual_ = true;
                }

                // 4e. 收集所有DC中RC>0的列
                std::vector<PricingResult> positiveCols;
                bool pricingCompleted = true;
                int pricingCalls = 0;  // 本次调定价的次数
                double maxRC = -1e100;
                int rcPositiveCount = 0;
                int maxColsPerDC = config ? config->pricing_max_cols_per_dc : 1;
                // Hysteresis: once a node has exhibited genuine tailing-off,
                // keep multi-column pricing active for the remainder of this
                // node's CG solve.
                // A child node starts a fresh solve (and therefore starts at K=1).
                // Immediate K=3 -> K=1 fallback caused repeated mode oscillation
                // and failed to escape degeneracy on the w=0.5 large instance.
                bool adaptiveMultiNow =
                    adaptiveMultiActive
                    || (config && config->pricing_adaptive_cols
                        && stallCount >= std::max(
                            1, config->pricing_adaptive_stall_iterations));
                if (adaptiveMultiNow) {
                    maxColsPerDC = std::max(
                        maxColsPerDC,
                        std::max(1, config->pricing_adaptive_max_cols));
                }
                std::vector<int> maxColsByDC(numDC, maxColsPerDC);
                if (config && config->pricing_per_dc_by_w && inst) {
                    int highWCols = std::max(
                        maxColsPerDC,
                        std::max(1, config->pricing_high_w_max_cols));
                    for (int j = 0; j < numDC && j < (int)inst->w.size(); j++) {
                        if (inst->w[j] >= config->pricing_high_w_threshold) {
                            maxColsByDC[j] = highWCols;
                        }
                    }
                }
                if (adaptiveMultiNow != adaptiveMultiActive) {
                    adaptiveMultiActive = adaptiveMultiNow;
                    if (logger) {
                        logger->log(
                            2,
                            "[CG-ADAPT-K] mode="
                                + std::string(adaptiveMultiNow ? "multi" : "single")
                                + " K=" + std::to_string(maxColsPerDC)
                                + " stall=" + std::to_string(stallCount)
                                + " iter=" + std::to_string(totalIterations));
                    }
                }

                if (numDC > 1 && config && config->parallel_pricing && pricingPool_) {
                    // 并行求解各DC的定价子问题。
                    // 关键：线程 j 使用持久成员 pricingSolvers_[j]，其缓存(cachedBestCols)
                    // 跨迭代保留；每个DC下标只被一个线程访问，天然无锁。
                    // 线程池 pricingPool_ 全程复用，避免每轮建销线程。
                    auto parStart = std::chrono::steady_clock::now();
                    std::vector<std::future<std::vector<PricingResult>>> pFutures(numDC);
                    for (int j = 0; j < numDC; j++) {
                        int colsForDC = maxColsByDC[j];
                        pFutures[j] = pricingPool_->enqueue(
                            [this, j, &dualVec, &branchState, colsForDC,
                             deadline]() {
                                if (colsForDC <= 1) {
                                    std::vector<PricingResult> r;
                                    r.push_back(
                                        pricingSolvers_[j].solve(
                                            j, dualVec, &branchState, deadline));
                                    return r;
                                }
                                return pricingSolvers_[j].solveMulti(
                                    j, dualVec, colsForDC, &branchState,
                                    deadline);
                            });
                    }
                    for (int j = 0; j < numDC; j++) {
                        std::vector<PricingResult> prs = pFutures[j].get();
                        if (!prs.empty() && !prs.front().completed) {
                            pricingCompleted = false;
                            continue;
                        }
                        pricingCalls++;
                        // 单列模式:首列可能为负列(未过滤);多列模式:solveMulti 只返回正列。
                        // 统一按 rc_eps 过滤,与串行路径口径一致。
                        for (auto& pr : prs) {
                            if (pr.reducedCost > maxRC) maxRC = pr.reducedCost;
                        }
                        if (!prs.empty() && prs[0].reducedCost > rc_eps) {
                            rcPositiveCount++;
                            for (auto& pr : prs) {
                                if (pr.reducedCost > rc_eps) {
                                    positiveCols.push_back(std::move(pr));
                                }
                            }
                        }
                    }
                    // 并行段墙钟计时：并行时各 solver 计时重叠，此处按整批墙钟累计真实耗时。
                    parallelPricingWallTime_ += std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - parStart).count();
                } else {
                    for (int j = 0; j < numDC; j++) {
                        int colsForDC = maxColsByDC[j];
                        if (colsForDC <= 1) {
                            PricingResult pr = pricingSolvers_[j].solve(
                                j, dualVec, &branchState, deadline);
                            pricingCalls++;
                            if (!pr.completed) {
                                pricingCompleted = false;
                                break;
                            }
                            if (pr.reducedCost > maxRC) maxRC = pr.reducedCost;
                            if (pr.reducedCost > rc_eps) {
                                rcPositiveCount++;
                                positiveCols.push_back(pr);
                            }
                        } else {
                            std::vector<PricingResult> prs =
                                pricingSolvers_[j].solveMulti(
                                    j, dualVec, colsForDC, &branchState,
                                    deadline);
                            pricingCalls++;
                            if (!prs.empty() && !prs.front().completed) {
                                pricingCompleted = false;
                                break;
                            }
                            // 首列为该DC最优列，用于维持 maxRC / rcPositiveCount 的原有口径
                            if (!prs.empty()) {
                                if (prs[0].reducedCost > maxRC) maxRC = prs[0].reducedCost;
                                rcPositiveCount++;
                                for (auto& pr : prs) positiveCols.push_back(std::move(pr));
                            }
                        }
                    }
                }
                if (!pricingCompleted || (deadline && deadline->expired())) {
                    return finishTimeLimit();
                }
                lastIterPricingCalls_ = pricingCalls;
                nodePositiveCandidates += static_cast<long long>(positiveCols.size());
                if (maxRC > nodeMaxReducedCost) nodeMaxReducedCost = maxRC;

                // 4e2. tailing-off 早停触发：连续多轮改善不足，视为已实质收敛。
                //      此时 result 已在整数性检查段填充完整，可直接返回当前 LP 解。
                if (cgEarlyStop && stallCount >= stallLimit) {
                    if (logger) {
                        std::string msg = "列生成 tailing-off 早停（连续 "
                            + std::to_string(stallLimit) + " 轮改善<"
                            + std::to_string(stallTol) + "），共 "
                            + std::to_string(totalIterations) + " 轮迭代";
                        if (consoleProgress_) logger->progress(2, msg);
                        else logger->log(2, msg);
                    }
#ifdef CG_DIAG
                    {
                        std::string timeMsg = "[DIAG-TIME] CG早停 iters="
                            + std::to_string(totalIterations)
                            + " cgTime=" + std::to_string(cgTime)
                            + " cplexSolve=" + std::to_string(cplexSolveTime)
                            + " pricing=" + std::to_string(getPricingTime())
                            + " cplexBuild=" + std::to_string(cplexBuildTime);
                        if (logger) logger->log(2, timeMsg);
                    }
#endif
                    auto end = std::chrono::steady_clock::now();
                    cgTime += std::chrono::duration<double>(end - start).count();
                    logNodeDiagnostic("TAILING_OFF");
                    return result;
                }

                // 4e3. 方案A mis-pricing 保护：平滑对偶下找不到正 RC 列时不能直接判收敛
                //      （平滑对偶≠真实对偶，可能假收敛）。回退用真实 LP 对偶 price[]
                //      再定价校验：仅当真实对偶下也无正 RC 列，才是数学上的真收敛。
                if (positiveCols.empty() && smoothAlpha > 0.0 && hasSmoothedDual_) {
                    std::vector<double> trueDual(numConstraints);
                    for (int i = 0; i < numConstraints; i++) trueDual[i] = price[i];
                    double trueMaxRC = -1e100;
                    for (int j = 0; j < numDC; j++) {
                        PricingResult pr = pricingSolvers_[j].solve(
                            j, trueDual, &branchState, deadline);
                        pricingCalls++;
                        if (!pr.completed) return finishTimeLimit();
                        if (pr.reducedCost > trueMaxRC) trueMaxRC = pr.reducedCost;
                        if (pr.reducedCost > rc_eps) positiveCols.push_back(pr);
                    }
                    lastIterPricingCalls_ = pricingCalls;
                    if (trueMaxRC > maxRC) maxRC = trueMaxRC;
                    // 真实对偶下仍出现正列 = mis-pricing（平滑造成假收敛）。
                    // 自适应稳定化的核心：不再只重置参考点，而是【一步关闭平滑】——
                    // α 直接归零并清空平滑历史，本节点起改用真实对偶继续列生成。
                    //   · 健康算例几乎不进这里 → α 维持初值，继续享受平滑加速；
                    //   · 退化算例(如 w=0)一旦进这里 → α 立即归零，本节点及全树后续
                    //     彻底退化为无平滑列生成（有数学收敛保证、列池不再被错向次优列
                    //     污染），从根本上消除 cycling 死循环与 BB 分支爆炸，无需按 w 特判。
                    if (!positiveCols.empty()) {
                        for (int i = 0; i < numConstraints; i++) smoothedDual_[i] = trueDual[i];
                        double prevAlpha = currentSmoothAlpha_;
                        currentSmoothAlpha_ *= kSmoothAlphaShrink;   // kSmoothAlphaShrink=0.0 → 一步归零
                        if (currentSmoothAlpha_ < kSmoothAlphaFloor) {
                            currentSmoothAlpha_ = 0.0;
                            hasSmoothedDual_ = false;   // 关平滑后清历史，下轮直接用真实对偶
                        }
                        if (logger) {
                            std::string msg = "检测到 mis-pricing（平滑假收敛），已一步关闭平滑 α: "
                                + std::to_string(prevAlpha) + " -> "
                                + std::to_string(currentSmoothAlpha_)
                                + "（本节点及全树后续退化为无平滑收敛）";
                            logger->log(2, msg);
                        }
                    }
                }

                // 4f. 收敛判断
                if (positiveCols.empty()) {
                    result.certifiedOptimal = true;
                    if (logger) {
                        if (consoleProgress_)
                            logger->progress(2, "列生成收敛，共 " + std::to_string(totalIterations) + " 轮迭代");
                        else
                            logger->log(2, "列生成收敛，共 " + std::to_string(totalIterations) + " 轮迭代");
                    }
                    auto end = std::chrono::steady_clock::now();
                    cgTime += std::chrono::duration<double>(end - start).count();
                    logNodeDiagnostic("CERTIFIED");
                    return result;
                }

                // 4g. 一次性加入所有RC>0的列（加速收敛）
                if (logger) {
                    std::string cgMsg = "CG iter " + std::to_string(totalIterations)
                        + ": obj=" + std::to_string(objValue)
                        + ", maxRC=" + std::to_string(maxRC)
                        + ", +" + std::to_string(positiveCols.size()) + " cols";
                    if (consoleProgress_)
                        logger->progress(2, cgMsg);
                    else
                        logger->log(2, cgMsg);
                }
                int addedThisIter = 0;   // 本轮有效进展列数（新列 + 被更新系数的重复列）
                int newColsThisIter = 0; // 本轮真正新增(去重后 push_back)的列数
                // 本轮“真正能改进”的列数：新列，或“重复但尚未顶到上界(z<1)且利润确有提升”的列。
                // 关键：RMP 中列变量 z∈[0,1]。定价按“无上界”的口径算 RC，对已顶上界(z≈1)的
                // 重复列会反复报大 RC（想再增大却受上界卡死），这是最大化问题在上界处的合法
                // 最优状态，并非“还能改进”。若本轮 RC>0 的列全为此类，即无任何列可真正改进，
                // 应判定收敛，避免定价空喊、RMP 加无可加的死循环空转。
                int effectiveImproveCols = 0;
#ifdef CG_DIAG
                auto addColStart = std::chrono::steady_clock::now();
#endif
                for (const auto& colResult : positiveCols) {
                    int optJ = colResult.dcIndex;
                    const std::vector<int>& optS = colResult.optimal_S;
                    double optPj = colResult.optimal_pj;
                    double newProfit = objFormula.columnProfit(*inst, optJ, optS, optPj, inst->w[optJ]);

                    // 防重复列：若该列(S)已存在于该DC的列集合，跳过。
                    // 定价子问题数值抖动可能反复返回同一列(RC>eps但列相同)，
                    // 不去重会导致列生成永不收敛（5×20 卡死的根因）。
                    bool duplicated = false;
                    int dupPos = -1;
                    for (int __e = 0; __e < (int)j_S[optJ].size(); __e++) {
                        if (j_S[optJ][__e] == optS) {
                            duplicated = true;
                            dupPos = __e;
                            break;
                        }
                    }
                    if (duplicated) {
                        // 该列(S)已在 RMP 中。定价按“无上界”口径算得正 RC，但 RMP 里 z∈[0,1]。
                        // 先看它当前在 RMP 中的取值：若已顶到上界(z≈1)，则其正 RC 是“想再增大却
                        // 被上界卡死”的合法最优状态，并非“还能改进”——不更新系数、不计为有效改进，
                        // 直接跳过（否则会陷入定价空喊、RMP 加无可加的死循环空转）。
                        double dupVal = 0.0;
                        try {
                            dupVal = cplex.getValue(z_j[optJ][dupPos]);
                        } catch (const IloException&) {
                            dupVal = 0.0;
                        }
                        (void)dupVal;

                        // 相同服务集合只保留利润最高的价格。只要新价格提高列系数，
                        // 就更新 RMP；DC 约束本身负责 z≤1，无需检查冗余变量上界。
                        if (dupPos >= 0
                            && newProfit > profits[optJ][dupPos] + rc_eps) {
                            totalProfit.setLinearCoef(z_j[optJ][dupPos], newProfit);
                            profits[optJ][dupPos] = newProfit;
                            p_j[optJ][dupPos] = optPj;
                            addedThisIter++;
                            effectiveImproveCols++;
                        }
                        continue;
                    }

                    IloNumColumn col = totalProfit(newProfit);
                    for (int n = 0; n < numRetailer; n++) {
                        if (optS[n] == 1) {
                            col = col + Fill[n](1);
                        }
                    }
                    int jIdx = numRetailer + optJ;
                    col = col + Fill[jIdx](1);
                    z_j[optJ].add(IloNumVar(
                        col, 0, IloInfinity, ILOFLOAT));

                    j_S[optJ].push_back(optS);
                    p_j[optJ].push_back(optPj);
                    profits[optJ].push_back(newProfit);
                    totalColumns++;
                    addedThisIter++;
                    newColsThisIter++;
                    effectiveImproveCols++;  // 真正新增的列必然是有效改进
                }
#ifdef CG_DIAG
                dbgAddColTimeTotal_ += std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - addColStart).count();
#endif
                nodeNewColumns += newColsThisIter;
                nodeDuplicateCandidates +=
                    static_cast<long long>(positiveCols.size() - newColsThisIter);
                nodeUpdatedColumns += addedThisIter - newColsThisIter;

                // [临时诊断] 尾部空转分析：对比"RC>0列/真新列/真能改进列"三个数，
                // 判断尾部是"持续产出新结构在慢慢爬"还是"反复产出无效列在空转"。
                if (logger) {
                    logger->log(2, "  [DIAG] iter=" + std::to_string(totalIterations)
                        + " posCols=" + std::to_string(positiveCols.size())
                        + " newCols=" + std::to_string(newColsThisIter)
                        + " effImprove=" + std::to_string(effectiveImproveCols)
                        + " updated=" + std::to_string(addedThisIter - newColsThisIter));
                }

                // 基于列上界的收敛判据：
                // RMP 中列变量 z∈[0,1]，约束为 Σz≤1（集合覆盖松弛）。定价子问题按“无上界”的
                // 口径计算 RC，对已顶到上界(z≈1)的重复列会反复报出大 RC——但对最大化问题而言，
                // 变量顶在上界处 RC 允许为正，这是合法的最优性状态，并非“还能改进”。
                // 因此本轮若没有任何“真正能改进”的列(effectiveImproveCols==0)——即所有 RC>0 的列
                // 都是已顶上界、加无可加的重复列——则已达当前列池的 LP 最优，判定收敛并返回，
                // 避免定价空喊、RMP 加无可加的死循环空转（此前空转至 2000 轮硬上限的根因）。
                if (effectiveImproveCols == 0) {
                    // 4h. 列池顶上界"假收敛"出口的平滑保护（w=0 退化算例 BB 分支爆炸的真正根治）：
                    //   平滑对偶变化平缓 → 定价子问题反复生成同一批列结构(S) → 列池早早顶上界，
                    //   effectiveImproveCols 归零 → 从这里"假收敛"返回，停在分数解 LP（比真最优低）
                    //   → BB 靠分支恢复整数解 → 分支爆炸。此出口此前完全没有平滑保护（4c 段的
                    //   mis-pricing 保护只覆盖 positiveCols.empty() 出口，退化算例每轮都有正列永不满足）。
                    //   这里在真正返回前用真实对偶 price[] 再定价一次校验：若真实对偶下能生成列池
                    //   尚不存在的【新列结构】，说明平滑困住了列结构探索，此非真收敛——一步关闭平滑，
                    //   本节点起改用真实对偶继续探索新结构（continue），直至列池真正顶上界收敛。
                    //   · 健康算例：平滑下已探索充分，真实对偶也无新结构 → 判真收敛 return，加速不受影响；
                    //   · 退化算例：真实对偶持续引入新结构 → 关平滑后像 smoothOFF 爬到整数解，消除 BB 爆炸。
                    if (currentSmoothAlpha_ > 0.0 && hasSmoothedDual_) {
                        std::vector<double> trueDual(numConstraints);
                        for (int i = 0; i < numConstraints; i++) trueDual[i] = price[i];
                        bool foundNewStructure = false;
                        for (int j = 0; j < numDC && !foundNewStructure; j++) {
                            PricingResult pr = pricingSolvers_[j].solve(
                                j, trueDual, &branchState, deadline);
                            pricingCalls++;
                            if (!pr.completed) return finishTimeLimit();
                            if (pr.reducedCost > rc_eps) {
                                // 仅当该列(S)是列池中不存在的【新结构】才算真正可继续探索，
                                // 已存在的重复列（无论是否顶上界）不计——避免与顶上界列混淆。
                                bool exists = false;
                                for (const auto& existingS : j_S[pr.dcIndex]) {
                                    if (existingS == pr.optimal_S) { exists = true; break; }
                                }
                                if (!exists) foundNewStructure = true;
                            }
                        }
                        lastIterPricingCalls_ = pricingCalls;
                        if (foundNewStructure) {
                            for (int i = 0; i < numConstraints; i++) smoothedDual_[i] = trueDual[i];
                            double prevAlpha = currentSmoothAlpha_;
                            currentSmoothAlpha_ = 0.0;   // 一步关闭平滑
                            hasSmoothedDual_ = false;    // 清平滑历史，下轮直接用真实对偶
                            if (logger) {
                                std::string msg = "检测到平滑困住列结构探索（列池顶上界假收敛，"
                                    "但真实对偶下仍有未探索的新列结构），已一步关闭平滑 α: "
                                    + std::to_string(prevAlpha) + " -> 0"
                                    + "（本节点及全树后续用真实对偶继续探索新结构）";
                                logger->log(2, msg);
                            }
                            continue;   // 关平滑后继续列生成，用真实对偶探索新结构，勿在此假收敛返回
                        }
                    }
                    if (logger) {
                        std::string msg = "列生成收敛（本轮所有 RC>0 的列均已顶上界，无可真正改进的列），共 "
                                          + std::to_string(totalIterations) + " 轮迭代";
                        if (consoleProgress_) logger->progress(2, msg);
                        else logger->log(2, msg);
                    }
                    // 定价仍报告正 RC，却没有任何可新增或可更新列，说明定价与
                    // RMP 列池尚未形成可验证的一致最优性证书。精确 BP 不允许
                    // 把这种停滞冒充收敛。
                    result.certifiedOptimal = false;
                    result.iterationLimitReached = true;
#ifdef CG_DIAG
                    {
                        std::string timeMsg = "[DIAG-TIME] CG退出 iters="
                            + std::to_string(totalIterations)
                            + " cgTime=" + std::to_string(cgTime)
                            + " cplexSolve=" + std::to_string(cplexSolveTime)
                            + " pricing=" + std::to_string(getPricingTime())
                            + " cplexBuild=" + std::to_string(cplexBuildTime);
                        if (logger) logger->log(2, timeMsg);
                    }
#endif
                    {
                        auto end = std::chrono::steady_clock::now();
                        cgTime += std::chrono::duration<double>(end - start).count();
                    }
                    logNodeDiagnostic("NO_EFFECTIVE_COLUMN");
                    return result;
                }
            }

        } catch (const IloException& ex) {
            std::cerr << "[ColumnGeneration] CPLEX异常: " << ex.getMessage() << std::endl;
            result.feasible = false;
        } catch (const std::exception& ex) {
            std::cerr << "[ColumnGeneration] 异常: " << ex.what() << std::endl;
            result.feasible = false;
            }

            // 达到迭代上限时，当前 RMP 解没有完成 reduced-cost 最优性认证。
            // 它不能作为分支定界节点的有效 LP 上界。
            result.iterationLimitReached = true;
            result.certifiedOptimal = false;
            if (logger) {
                logger->log(2, "列生成达到最大迭代次数，节点LP未完成最优性认证");
            }
            auto end = std::chrono::steady_clock::now();
        cgTime += std::chrono::duration<double>(end - start).count();
        logNodeDiagnostic(result.feasible ? "ITERATION_LIMIT" : "EXCEPTION");
        return result;
    }
};

#endif // COLUMN_GENERATION_H
