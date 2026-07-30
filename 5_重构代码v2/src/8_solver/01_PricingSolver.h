#ifndef PRICING_SOLVER_H
#define PRICING_SOLVER_H

#include "../10_common/Instance.h"
#include "../10_common/Types.h"
#include "../10_common/Config.h"
#include "../7_formula/03_TransportCost.h"
#include "../7_formula/04_InventoryCost.h"
#include "../7_formula/08_Objective.h"
#include "../4_logger/Logger.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <chrono>
#include <unordered_set>
#include <set>
#include <cstdint>
#include <string>
#include <array>

/**
 * bitmask（uint64 字数组）哈希器
 *
 * 定价子问题两点枚举的候选去重用。std::vector<uint64_t> 无标准哈希，
 * 这里用 FNV-1a 风格的 hash_combine 逐字混合，使其可作 unordered_set 的键。
 * 相比 std::set 的红黑树 O(log n) 有序插入，O(1) 均摊且缓存更友好。
 */
struct MaskHash {
    std::size_t operator()(const std::vector<uint64_t>& mask) const noexcept {
        std::size_t h = 1469598103934665603ULL;  // FNV offset basis
        for (uint64_t word : mask) {
            h ^= static_cast<std::size_t>(word);
            h *= 1099511628211ULL;  // FNV prime
        }
        return h;
    }
};

/**
 * 定价子问题求解器（Pricing Problem）
 *
 * 对应论文中分支定价框架的定价子问题：
 * 对每个DC j，在给定对偶变量 π 的情况下，寻找使检验数（reduced cost）最大
 * 的列（S, p_j），即求解：
 *
 *   min_{S ⊆ N, p_j ∈ [0, v_i]}  RC_j(S, p_j) = -R_j(S, p_j) + Σ_{i∈S} π_i + π_{N_DC+j}
 *
 * 支持两种算法（通过配置 use_paper_algorithm3 切换）：
 *   1. 简化凸包枚举（默认）—— 使用 (xi,yi)=(-μ_i/Ai, -σ²_i/Ai)，枚举所有两点组合上下半平面
 *   2. 论文 Algorithm 3（开关打开）—— 使用累积 Ḡ/K̄ 计算坐标，余弦相似度分类 I1/I2/I3，12个候选集
 *
 * 两种算法都通过最终调用 columnProfit() 计算真实检验数来选取最优候选，
 * 因此结果应一致。Algorithm 3 的候选集更精确，理论上不漏解。
 */

class PricingSolver {
private:
    const Instance* inst;
    const Config* config;
    Logger* logger;
    double rc_eps;
    double pricingTime;

    // 公共的 Ai 计算函数（两种算法共用）
    struct RetailerFeature {
        int idx;           // 零售商原始索引
        double Ai;         // 线性贡献 Ā_i(p_j)
        double mu;         // 需求 μ_i
        double variance;   // 方差 σ²_i
    };

    // ===== 方案1优化：定价子问题多列缓存 =====
    // 对每个DC j，缓存最近找到的 K 个好列（不再只存1列）。
    // 对偶价格每轮仅微变，上几轮的好列本轮往往仍是正检验数列，
    // 快路径遍历这 K 列取 RC 最大者命中，即可跳过整段凸包枚举。
    struct CachedColumn {
        std::vector<int> S;     // 服务集合
        double p_j;             // 批发价
        double reducedCost;     // 当时的检验数
        int iteration;          // 缓存时的迭代号（用于LRU淘汰）
    };
    // 每个DC一个列表，最多保留 kMaxCachedCols 列，满则淘汰最旧（iteration 最小）
    static const int kMaxCachedCols = 5;
    std::vector<std::vector<CachedColumn>> cachedBestCols;  // 每个DC一个多列列表
    int pricingIterCounter = 0;                             // 全局迭代计数器
    // solveMulti() 请求一次完整定价时，solve() 在同一轮价格扫描中顺带收集
    // 每个候选价格的最优正列，再从中保留 K 个唯一服务集合。注意这不是所有
    // (price, service set) 组合的严格全局 Top-K；其用途是无损地附加多列加速，
    // 而 solve() 返回的全局最佳列仍负责精确定价与收敛认证。
    int requestedFreshCols_ = 1;
    std::vector<PricingResult> lastFreshCandidates_;

public:
    PricingSolver() : inst(nullptr), config(nullptr), logger(nullptr),
                      rc_eps(1e-6), pricingTime(0.0) {
        // 延迟初始化缓存（等 setInstance 后）
    }

    void setInstance(const Instance& instance) {
        inst = &instance;
        cachedBestCols.assign(inst->numDC, std::vector<CachedColumn>());
    }
    void setConfig(const Config& cfg) { config = &cfg; }
    void setRCEps(double eps) { rc_eps = eps; }
    void setLogger(Logger* log) { logger = log; }

    double getPricingTime() const { return pricingTime; }

    /** 重置统计和缓存（在多次BP调用间调用） */
    void resetStats() {
        pricingTime = 0.0;
        pricingIterCounter = 0;
        for (auto& colList : cachedBestCols) {
            colList.clear();
        }
    }

    /**
     * 求解DC j的定价子问题（带缓存优化）
     *
     * @param j DC索引
     * @param dual 对偶变量数组（长度 = 零售商数 + DC数）
     * @return 最优列（S, p_j）和检验数
     */
    PricingResult solve(int j, const std::vector<double>& dual,
                        const BranchState* branch = nullptr) {
        auto start = std::chrono::steady_clock::now();
        pricingIterCounter++;

        PricingResult result(inst->numRetailer);
        result.dcIndex = j;
        result.reducedCost = -DBL_MAX;
        lastFreshCandidates_.clear();
        auto collectFresh = [&](const PricingResult& candidate) {
            if (requestedFreshCols_ <= 1 || candidate.reducedCost <= rc_eps) return;
            // RMP 按服务集合 S 去重；同一 S 只保留当前对偶下 RC 更高的价格。
            for (auto& existing : lastFreshCandidates_) {
                if (existing.optimal_S == candidate.optimal_S) {
                    if (candidate.reducedCost > existing.reducedCost) {
                        existing = candidate;
                    }
                    return;
                }
            }
            lastFreshCandidates_.push_back(candidate);
            if ((int)lastFreshCandidates_.size() > requestedFreshCols_) {
                auto worst = std::min_element(
                    lastFreshCandidates_.begin(), lastFreshCandidates_.end(),
                    [](const PricingResult& a, const PricingResult& b) {
                        return a.reducedCost < b.reducedCost;
                    });
                lastFreshCandidates_.erase(worst);
            }
        };

        // ===== 无损缓存：取当前 dual 下最好的缓存列作为 incumbent，再执行完整定价 =====
        // 分支约束下：缓存列必须仍满足约束，否则不能复用。
        if (j < (int)cachedBestCols.size() && !cachedBestCols[j].empty()) {
            ObjectiveFormula objFormula(*config);
            double bestRc = rc_eps;   // 只接受严格大于阈值的列
            int bestPos = -1;
            for (int c = 0; c < (int)cachedBestCols[j].size(); c++) {
                const auto& cached = cachedBestCols[j][c];
                if (branch != nullptr && !branch->isColumnFeasible(j, cached.S)) {
                    continue;  // 分支约束下不可行，跳过
                }
                if (!RevenueFormula::serviceSetAcceptsPrice(
                        *inst, cached.S, cached.p_j)) {
                    continue;
                }
                double profit = objFormula.columnProfit(*inst, j, cached.S, cached.p_j,
                    (j < (int)inst->w.size()) ? inst->w[j] : 0.0);
                double rc = profit;
                for (int i = 0; i < inst->numRetailer; i++) {
                    if (cached.S[i] == 1) rc -= dual[i];
                }
                rc -= dual[inst->numRetailer + j];
                if (rc > bestRc) {
                    bestRc = rc;
                    bestPos = c;
                }
            }
            if (bestPos >= 0) {
                // 命中：用该缓存列抬高 incumbent，帮助后续上界剪枝；不能跳过完整定价
                auto& hit = cachedBestCols[j][bestPos];
                result.reducedCost = bestRc;
                result.optimal_pj = hit.p_j;
                result.optimal_S = hit.S;
                result.dcIndex = j;
                // 更新命中列的检验数与迭代号（用于LRU）
                hit.reducedCost = bestRc;
                hit.iteration = pricingIterCounter;
            }
        }

        {
            // A cache hit is only an incumbent for exact pricing, not a proof that
            // the cached column maximizes reduced cost under the current duals.
            // Always run the complete pricing search below.  The incumbent still
            // accelerates the lossless upper-bound pruning through result.reducedCost.
            // 候选价格为各零售商的保留价
            std::vector<double> candidate_prices;
            for (int i = 0; i < inst->numRetailer; i++) {
                candidate_prices.push_back(inst->reservePrice[i]);
            }
            // 若右分支要求 DC j 服务若干零售商，统一价格必须不超过这些
            // 零售商中的最低保留价，否则 S_i=1 与“接受服务”的模型含义冲突。
            if (branch) {
                double forcedPriceCap = DBL_MAX;
                bool hasForcedRetailer = false;
                for (const auto& rb : branch->retailerBranches) {
                    if (rb.dcIndex == j && rb.value == 1
                        && rb.retailerIndex >= 0
                        && rb.retailerIndex < inst->numRetailer) {
                        hasForcedRetailer = true;
                        forcedPriceCap = std::min(
                            forcedPriceCap,
                            inst->reservePrice[rb.retailerIndex]);
                    }
                }
                if (hasForcedRetailer) {
                    candidate_prices.erase(
                        std::remove_if(
                            candidate_prices.begin(), candidate_prices.end(),
                            [forcedPriceCap](double p) {
                                return p > forcedPriceCap + 1e-9;
                            }),
                        candidate_prices.end());
                    // forcedPriceCap 本身来自某个零售商保留价，正常情况下已在
                    // candidate_prices 中；显式补入可防未来候选价生成规则变化。
                    candidate_prices.push_back(forcedPriceCap);
                }
            }
            // 去重
            std::sort(candidate_prices.begin(), candidate_prices.end());
            candidate_prices.erase(
                std::unique(candidate_prices.begin(), candidate_prices.end()),
                candidate_prices.end()
            );

            // 对每个候选价格求解最优服务集合，取RC最大的
            // 【优化3】严格无损上界剪枝：先算 O(n) 的 RC 上界，
            //   若上界 ≤ 当前已找到的最优 RC，则该价不可能更优，
            //   直接跳过昂贵的凸包枚举。上界忽略非线性库存成本
            //   （恒正、被减去）故偏高，仍 ≥ 真实 RC，剪枝不漏最优解。
            // 定价算法三值选择：0=Simplified / 1=Algorithm3 / 2=ConvexHull
            int algoMode = config ? config->pricing_algorithm : 0;
            // 上界剪枝仅在非论文精确版启用（Algorithm3 需完整枚举以保精确对比）
            bool enableUpperBound = (algoMode != 1);
            // 【优化4：上界降序早停】按 RC 上界降序遍历候选价格。
            //   上界剪枝的效果依赖尽早建立高 bestRC——先处理上界最高的价格，
            //   能更快抬高 result.reducedCost，从而剪掉更多低上界价格，
            //   减少昂贵的凸包/单纯形枚举调用。严格无损：只改遍历顺序，
            //   最终仍取全局 RC 最大的列。同时复用此处算得的上界，避免循环内
            //   重复调用 computePriceUpperBound。Algorithm3 保持原枚举顺序。
            if (enableUpperBound) {
                // 【优化5】价格无关基底预算一次，供本轮所有候选价格复用，
                //   省掉 computePriceUpperBound / solveForPrice_Simplified 内
                //   对分段运输公式的重复计算。纯栈上局部，无并发风险。
                FeatureBase featBase = computeFeatureBase(j, dual);
                std::vector<std::pair<double, double>> priced;  // (上界, 价格)
                priced.reserve(candidate_prices.size());
                for (double price : candidate_prices) {
                    priced.emplace_back(
                        computePriceUpperBound(j, price, dual, &featBase), price);
                }
                std::sort(priced.begin(), priced.end(),
                          [](const std::pair<double, double>& a,
                             const std::pair<double, double>& b) {
                              return a.first > b.first;  // 上界降序
                          });
                for (const auto& pr : priced) {
                    const double stopThreshold =
                        (requestedFreshCols_ <= 1)
                            ? result.reducedCost
                            : rc_eps;
                    if (pr.first <= stopThreshold) {
                        // 单列模式按当前全局最佳 RC 剪枝。多列模式不能用第一名
                        // 的 RC 提前退出，否则会漏掉其他价格下的附加正列；这里只
                        // 在后续价格的严格上界已不可能产生正列时才停止。
                        break;
                    }
                    double price = pr.second;
                    PricingResult subResult;
                    if (algoMode == 2) {
                        PricingResult hullCandidate =
                            solveForPrice_ConvexHull(j, price, dual, branch);
                        PricingResult verifiedCandidate =
                            solveForPrice_Simplified(
                                j, price, dual, branch, &featBase);
                        subResult =
                            (hullCandidate.reducedCost
                                 > verifiedCandidate.reducedCost)
                                ? hullCandidate
                                : verifiedCandidate;
                    } else {
                        subResult = solveForPrice_Simplified(j, price, dual, branch, &featBase);
                    }
                    collectFresh(subResult);
                    if (subResult.reducedCost > result.reducedCost) {
                        result = subResult;
                    }
                }
            } else {
                for (double price : candidate_prices) {
                    PricingResult subResult =
                        solveForPrice_Algorithm3(j, price, dual, branch);
                    collectFresh(subResult);
                    if (subResult.reducedCost > result.reducedCost) {
                        result = subResult;
                    }
                }
            }
            collectFresh(result);

            // 如果找到了正检验数列，加入多列缓存（去重 + LRU 淘汰）
            if (result.reducedCost > rc_eps && j < (int)cachedBestCols.size()) {
                auto& colList = cachedBestCols[j];
                // 去重：若已有相同列(相同S且相同p_j)，仅刷新其检验数与迭代号
                bool duplicate = false;
                for (auto& c : colList) {
                    if (c.p_j == result.optimal_pj && c.S == result.optimal_S) {
                        c.reducedCost = result.reducedCost;
                        c.iteration = pricingIterCounter;
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    CachedColumn nc;
                    nc.S = result.optimal_S;
                    nc.p_j = result.optimal_pj;
                    nc.reducedCost = result.reducedCost;
                    nc.iteration = pricingIterCounter;
                    if ((int)colList.size() < kMaxCachedCols) {
                        colList.push_back(std::move(nc));
                    } else {
                        // 淘汰 iteration 最小（最旧）的列
                        int oldestPos = 0;
                        for (int c = 1; c < (int)colList.size(); c++) {
                            if (colList[c].iteration < colList[oldestPos].iteration) {
                                oldestPos = c;
                            }
                        }
                        colList[oldestPos] = std::move(nc);
                    }
                }
            }
        }

        auto end = std::chrono::steady_clock::now();
        pricingTime += std::chrono::duration<double>(end - start).count();

        return result;
    }

    /**
     * 求解DC j的定价子问题，返回多个正检验数列（multi-column pricing）。
     *
     * solve() 仍执行完整定价并返回全局最优列；与此同时，在同一次候选价格扫描中
     * 收集“每个候选价格的最优列”中的 K 个唯一正列。附加列不再依赖历史
     * 缓存。它是多列加速候选集，不宣称是全部可行列的严格全局 Top-K。
     *
     * @param j DC索引
     * @param dual 对偶变量数组
     * @param maxCols 最多返回的列数（含最优列），<=1 时退化为单列
     * @param branch 分支约束
     * @return 按 RC 降序排列的多个正检验数列，首列为最优列。若无正列则返回空。
     */
    std::vector<PricingResult> solveMulti(int j, const std::vector<double>& dual,
                                          int maxCols,
                                          const BranchState* branch = nullptr) {
        std::vector<PricingResult> out;
        requestedFreshCols_ = std::max(1, maxCols);
        PricingResult best = solve(j, dual, branch);
        requestedFreshCols_ = 1;
        if (best.reducedCost <= rc_eps) {
            return out;  // 无正检验数列，直接返回空
        }
        out.push_back(best);
        if (maxCols <= 1) {
            return out;
        }

        // 使用刚才完整价格扫描得到的新鲜候选；按 S 去重，best 始终置于首位。
        std::vector<PricingResult> extras;
        for (const auto& candidate : lastFreshCandidates_) {
            if (candidate.optimal_S == best.optimal_S) continue;
            if (candidate.reducedCost > rc_eps) extras.push_back(candidate);
        }
        std::sort(extras.begin(), extras.end(),
                  [](const PricingResult& a, const PricingResult& b) {
                      return a.reducedCost > b.reducedCost;
                  });
        int room = maxCols - 1;
        for (int c = 0; c < (int)extras.size() && c < room; c++) {
            out.push_back(std::move(extras[c]));
        }
        return out;
    }

    /**
     * 对同一个价格用两种算法求解并对比结果
     * 用于验证 Algorithm 3 是否正确
     */
    bool verifyBothAlgorithms(int j, double p_j, const std::vector<double>& dual,
                              double& rcSimplified, double& rcAlgorithm3) {
        PricingResult r1 = solveForPrice_Simplified(j, p_j, dual);
        PricingResult r2 = solveForPrice_Algorithm3(j, p_j, dual);
        rcSimplified = r1.reducedCost;
        rcAlgorithm3 = r2.reducedCost;
        return std::abs(rcSimplified - rcAlgorithm3) < 1e-6;
    }

private:
    // ============================================================
    // 公共辅助方法
    // ============================================================

    /**
     * 计算所有零售商的 Ā_i(p_j)
     *
     * Ā_i(p_j) = (c(d_ij)+p̂·b̂_ij·(1-w_j) + c(a_j)+p̂·b̂_j·(1-w_j) - r_i(p_j)) · μ_i + λ_i
     *
     * 对应论文 Model-SM_j(p_j) 中的 Ā_i(p_j)
     *
     * 优化：trans_cost_sup_dc 不依赖 i，提到循环外面计算；
     *       每对 (i,j) 的 c(d_ij) 和 b̂_ij 是固定的，可提前缓存。
     */
    // 【优化5：价格无关基底缓存】
    // Ai(p_j) = (trans_dc_ret + trans_sup_dc − new_revenue(p_j)) · μ_i + dual[i]
    // 其中只有 new_revenue = (p_j>reserve_i)?0:p_j 依赖候选价格，其余全为
    // 价格无关基底。一次 solve(j,dual) 调用内 j/dual 固定、仅 p_j 在变，
    // 故把基底 baseAi_i = (trans_dc_ret + trans_sup_dc)·μ_i + dual[i] 预算一次，
    // 各候选价格只需 O(1) 减去 new_revenue·μ_i，省掉重复的分段运输公式调用。
    // 纯局部结构（栈上、随 solve 返回释放），不跨调用、不跨线程 → 零并发风险。
    struct FeatureBase {
        std::vector<double> baseAi;   // 价格无关部分 (trans+trans)·μ_i + dual[i]
    };

    FeatureBase computeFeatureBase(int j, const std::vector<double>& dual) const {
        double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
        FeatureBase base;
        base.baseAi.reserve(inst->numRetailer);

        // trans_cost_sup_dc 不依赖 i，提到循环外
        double trans_cost_sup_dc = TransportCostFormula::costSupplierToDC(inst->a_dist[j])
            + inst->p * (1.0 - w_j) * inst->b[j];

        for (int i = 0; i < inst->numRetailer; i++) {
            double trans_cost_dc_retail = TransportCostFormula::costDCToRetailer(inst->dist[i][j])
                + inst->p * (1.0 - w_j) * inst->bb[i][j];
            base.baseAi.push_back(
                (trans_cost_dc_retail + trans_cost_sup_dc) * inst->mu[i] + dual[i]);
        }
        return base;
    }

    // 快路径重载：复用价格无关基底，仅叠加价格相关项 −new_revenue·μ_i。
    std::vector<RetailerFeature> computeFeatures(int j, double p_j,
                                                 const std::vector<double>& dual,
                                                 const FeatureBase& base) const {
        std::vector<RetailerFeature> features;
        features.reserve(inst->numRetailer);
        for (int i = 0; i < inst->numRetailer; i++) {
            double new_revenue = (p_j > inst->reservePrice[i]) ? 0.0 : p_j;
            RetailerFeature f;
            f.idx = i;
            f.Ai = base.baseAi[i] - new_revenue * inst->mu[i];
            f.mu = inst->mu[i];
            f.variance = inst->variance[i];
            features.push_back(f);
        }
        return features;
    }

    std::vector<RetailerFeature> computeFeatures(int j, double p_j,
                                                   const std::vector<double>& dual) const {
        // 兼容旧调用点：内部先算基底再走快路径（单次调用无额外重复）。
        return computeFeatures(j, p_j, dual, computeFeatureBase(j, dual));
    }

    /**
     * Ḡ_j(D) = (√(2(F_j+g_j)(h+p̂ĥ(1-w_j)) + ½δw_j²) · √D
     *
     * 对应论文中 Ḡ_j(Σμ_i)：与需求相关的库存成本+投资成本
     */
    double computeGbar(int j, double totalDemand, double w_j) const {
        if (totalDemand <= 0) return 0.0;
        double inv_factor = InventoryCostFormula::inventoryCostFactor(
            inst->h, inst->p, inst->hat_h, w_j);
        double eoq_part = std::sqrt(2.0 * (inst->F[j] + inst->g[j]) * inv_factor * totalDemand);
        double invest_part = 0.0;
        if (config && config->use_invest_in_column) {
            invest_part = InvestmentCostFormula::compute(
                *inst, j, w_j, totalDemand, config->use_sqrt_investment,
                config->investment_exponent);
        }
        return eoq_part + invest_part;
    }

    /**
     * K̄_j(V) = (h + p̂ĥ(1-w_j)) · z_α · √(L_j · V)
     *
     * 对应论文中 K̄_j(Σσ²_i)：与方差相关的安全库存成本
     */
    double computeKbar(int j, double totalVariance, double w_j) const {
        if (totalVariance <= 0) return 0.0;
        double inv_factor = InventoryCostFormula::inventoryCostFactor(
            inst->h, inst->p, inst->hat_h, w_j);
        return inv_factor * inst->z_alpha * std::sqrt(inst->L[j] * totalVariance);
    }

    /**
     * 计算候选集 S 的完整检验数（用于最终评估）
     *
     * reducedCost = columnProfit - Σλ_i - λ_{|I|+j}
     */
    double computeReducedCost(const std::vector<int>& S, int j,
                               double p_j, double w_j,
                               const std::vector<double>& dual) const {
        ObjectiveFormula objFormula(*config);
        double profit = objFormula.columnProfit(*inst, j, S, p_j, w_j);
        double rc = profit;
        for (int i = 0; i < inst->numRetailer; i++) {
            if (S[i] == 1) rc -= dual[i];
        }
        rc -= dual[inst->numRetailer + j];
        return rc;
    }

    /**
     * 【优化3】计算候选价 p_j 的 RC 上界（严格无损剪枝用）
     *
     * 任意服务集合 S 的检验数：
     *   RC(S) = columnProfit(S, p_j) - Σ_{i∈S} λ_i - λ_DC
     * 在 computeFeatures 中，选中零售商 i 对 RC 的边际贡献恰为 -Ai，
     * 与 S 无关的固定项为 -facilityCost - λ_DC（空集时的 RC）。
     *
     * 因此该价可达到的 RC 上界 =
     *   空集RC + Σ_{i: Ai<0} (-Ai)
     * 这是紧上界：加入任何 Ai≥0 的零售商只会降低 RC。
     *
     * 上界 ≥ 该价下所有集合的真实 RC，故据此剪枝严格无损。
     * 代价仅 O(n)（一次 computeFeatures），远低于 O(n³) 的凸包枚举。
     *
     * @return 该候选价可能达到的最大检验数上界
     */
    double computePriceUpperBound(int j, double p_j,
                                  const std::vector<double>&dual,
                                  const FeatureBase* base = nullptr) const {
        double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
        auto features = base ? computeFeatures(j, p_j, dual, *base)
                             : computeFeatures(j, p_j, dual);

        // 空集列利润（S 全 0）：收入=0，仅含固定成本与运输/库存的常数部分。
        // columnProfit 对空集返回 -facilityCost（运输/库存对空集为 0）。
        std::vector<int> emptyS(inst->numRetailer, 0);
        ObjectiveFormula objFormula(*config);
        double emptyProfit = objFormula.columnProfit(*inst, j, emptyS, p_j, w_j);
        double upper = emptyProfit - dual[inst->numRetailer + j];

        // 累加所有正贡献零售商的边际贡献 -Ai（Ai<0 时为正）
        for (int i = 0; i < inst->numRetailer; i++) {
            if (features[i].Ai < 0) {
                upper += -features[i].Ai;
            }
        }
        return upper;
    }

    /**
     * 从 negative_idx 重建 S 向量
     */
    static std::vector<int> buildS(int numRetailer,
                                    const std::vector<int>& indices,
                                    const std::vector<int>& selected) {
        std::vector<int> S(numRetailer, 0);
        for (size_t t = 0; t < indices.size(); t++) {
            if (selected[t]) {
                S[indices[t]] = 1;
            }
        }
        return S;
    }

    // ============================================================
    // 算法1：简化凸包枚举（默认）
    // ============================================================

    /**
     * 简化凸包枚举算法（加速版）
     *
     * 使用 (xi, yi) = (-μ_i/Ai, -σ²_i/Ai)
     * 枚举所有两点组合的上下半平面，生成候选集
     * 使用预计算的边际贡献快速计算检验数
     */
    PricingResult solveForPrice_Simplified(int j, double p_j,
                                            const std::vector<double>& dual,
                                            const BranchState* branch = nullptr,
                                            const FeatureBase* base = nullptr) {
        PricingResult result(inst->numRetailer);
        result.dcIndex = j;
        result.optimal_pj = p_j;

        double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;

        auto features = base ? computeFeatures(j, p_j, dual, *base)
                             : computeFeatures(j, p_j, dual);

        // Embed retailer branching directly in the pricing problem.
        // -1 = free, 0 = forbidden, 1 = required.
        std::vector<int> fixedValue(inst->numRetailer, -1);
        if (branch) {
            for (const auto& rb : branch->retailerBranches) {
                if (rb.dcIndex == j && rb.retailerIndex >= 0
                    && rb.retailerIndex < inst->numRetailer) {
                    fixedValue[rb.retailerIndex] = rb.value;
                }
            }
        }

        // 收集 Ai < 0 的零售商，同时预计算每个候选零售商对列利润的
        // 【线性单位贡献】，供后续增量式检验数计算复用（优化 D）。
        //   locRevenue_t = r_i(p_j)·μ_i                       —— 收入线性项
        //   locTransport_t = (c_sup_dc + c_dc_ret)·μ_i
        //                    + p̂(1-w_j)(b̂_j + b̂_ij)·μ_i       —— 运输(含碳)线性项
        //   locMu_t / locVar_t 用于库存 EOQ/安全库存的非线性 √ 项
        //   locDual_t = dual[i]                               —— RMP 对偶线性项
        // 这些量与 columnProfit 各子公式逐项等价，故据此聚合严格无损。
        std::vector<int> negative_idx;
        std::vector<double> xi(inst->numRetailer);
        std::vector<double> yi(inst->numRetailer);

        // negative 局部单位量（下标与 negative_idx 对齐，长度 n_neg）
        std::vector<double> locRevenue, locTransport, locMu, locVar, locDual;
        negative_idx.reserve(inst->numRetailer);
        locRevenue.reserve(inst->numRetailer);
        locTransport.reserve(inst->numRetailer);
        locMu.reserve(inst->numRetailer);
        locVar.reserve(inst->numRetailer);
        locDual.reserve(inst->numRetailer);

        double forcedRevenue = 0.0;
        double forcedTransport = 0.0;
        double forcedMu = 0.0;
        double forcedVar = 0.0;
        double forcedDual = 0.0;

        double trans_cost_sup_dc = TransportCostFormula::costSupplierToDC(inst->a_dist[j]);
        double carbon_factor = inst->p * (1.0 - w_j);
        double carbon_sup_dc = inst->b[j];  // b̂_j

        for (int i = 0; i < inst->numRetailer; i++) {
            if (fixedValue[i] == 1) {
                double mu_i = inst->mu[i];
                double rev = ((p_j <= inst->reservePrice[i]) ? p_j : 0.0) * mu_i;
                double trans_lin = (trans_cost_sup_dc
                                    + TransportCostFormula::costDCToRetailer(inst->dist[i][j]))
                                   * mu_i;
                double trans_carbon =
                    carbon_factor * (carbon_sup_dc + inst->bb[i][j]) * mu_i;
                forcedRevenue += rev;
                forcedTransport += trans_lin + trans_carbon;
                forcedMu += mu_i;
                forcedVar += inst->variance[i];
                forcedDual += dual[i];
            } else if (fixedValue[i] != 0 && features[i].Ai < 0) {
                negative_idx.push_back(i);
                xi[i] = -static_cast<double>(features[i].mu) / features[i].Ai;
                yi[i] = -features[i].variance / features[i].Ai;

                double mu_i = inst->mu[i];
                double rev = ((p_j <= inst->reservePrice[i]) ? p_j : 0.0) * mu_i;
                double trans_lin = (trans_cost_sup_dc
                                    + TransportCostFormula::costDCToRetailer(inst->dist[i][j]))
                                   * mu_i;
                double trans_carbon = carbon_factor * (carbon_sup_dc + inst->bb[i][j]) * mu_i;

                locRevenue.push_back(rev);
                locTransport.push_back(trans_lin + trans_carbon);
                locMu.push_back(mu_i);
                locVar.push_back(inst->variance[i]);
                locDual.push_back(dual[i]);
            }
        }

        int n_neg = negative_idx.size();

        // 与 S 无关的常数项（提到候选循环外）：
        //   facilityCost = f_j + p̂·f̂_j·(1-w_j)
        //   inv_factor 库存成本因子，用于 EOQ/安全库存
        double facilityCost = FacilityCostFormula::totalFixedCost(*inst, j, w_j);
        double inv_factor = InventoryCostFormula::inventoryCostFactor(
            inst->h, inst->p, inst->hat_h, w_j);
        double dualDC = dual[inst->numRetailer + j];
        double constRC = -facilityCost - dualDC;  // 空集(D=0,库存=0) 的 RC

        // 【优化 A+B】候选服务集合只在 negative_idx 的局部下标(0..n_neg-1)上枚举：
        // Ai >= 0 的零售商选入只会降低检验数，绝不可能出现在最优 S 中，
        // 故无需为其分配全 R 维向量、无需参与去重。这样把原先 O(R) 的
        // 向量构造/遍历/去重全部降到 O(n_neg)，且用 bitmask(vector<uint64_t>)
        // 替代 R 长 std::string 作去重 key，消除大量堆分配。
        // 本优化严格无损：候选集合与原实现完全一致，只是表示方式更紧凑。
        constexpr int kBits = 64;
        int nWords = (n_neg + kBits - 1) / kBits;
        if (nWords < 1) nWords = 1;
        std::vector<std::vector<uint64_t>> localCandidates;  // 每个候选：局部下标的 bitmask

        auto emptyMask = [&]() { return std::vector<uint64_t>(nWords, 0ULL); };
        auto setBit = [&](std::vector<uint64_t>& mask, int localIdx) {
            mask[localIdx / kBits] |= (1ULL << (localIdx % kBits));
        };

        // 空集候选
        localCandidates.push_back(emptyMask());

        if (n_neg == 1) {
            auto one = emptyMask();
            setBit(one, 0);
            localCandidates.push_back(one);
        } else if (n_neg >= 2) {
            // 单元素集
            for (int t = 0; t < n_neg; t++) {
                auto single = emptyMask();
                setBit(single, t);
                localCandidates.push_back(single);
            }

            // 枚举所有两点组合（在局部下标上操作）
            for (int m = 0; m < n_neg - 1; m++) {
                for (int n = m + 1; n < n_neg; n++) {
                    int idx_m = negative_idx[m];
                    int idx_n = negative_idx[n];

                    double dx = xi[idx_n] - xi[idx_m];
                    if (std::abs(dx) < 1e-15) continue;
                    double k = (yi[idx_n] - yi[idx_m]) / dx;
                    double b = yi[idx_n] - k * xi[idx_n];

                    auto below = emptyMask();
                    auto above = emptyMask();

                    for (int t = 0; t < n_neg; t++) {
                        int idx_t = negative_idx[t];
                        double val = k * xi[idx_t] + b;
                        if (yi[idx_t] <= val + 1e-12) setBit(below, t);
                        if (yi[idx_t] >= val - 1e-12) setBit(above, t);
                    }

                    localCandidates.push_back(std::move(below));
                    localCandidates.push_back(std::move(above));
                }
            }
        }

        // 去重（bitmask 作 key）并计算检验数（优化 D：增量式聚合）。
        // 每个候选只遍历其【选中的 negative 位】累加 5 个标量，
        // 再按 columnProfit 的等价公式组合，避免展开全 R 维 + 6 次 O(R) 遍历。
        // 分支可行性与 optimal_S 输出仍需全 R 维，故仅在候选可能刷新最优、
        // 或需分支过滤时才按需展开一次，热路径开销降为 O(popcount)。
        std::vector<int> Sfull(inst->numRetailer, 0);
        std::unordered_set<std::vector<uint64_t>, MaskHash> seen;
        seen.reserve(localCandidates.size() * 2);  // 预留桶，减少 rehash
        for (const auto& mask : localCandidates) {
            if (!seen.insert(mask).second) continue;

            // 增量聚合选中位的线性量与需求/方差
            double sumRev = forcedRevenue;
            double sumTrans = forcedTransport;
            double sumMu = forcedMu;
            double sumVar = forcedVar;
            double sumDual = forcedDual;
            for (int t = 0; t < n_neg; t++) {
                if (mask[t / kBits] & (1ULL << (t % kBits))) {
                    sumRev += locRevenue[t];
                    sumTrans += locTransport[t];
                    sumMu += locMu[t];
                    sumVar += locVar[t];
                    sumDual += locDual[t];
                }
            }

            // 库存 EOQ + 安全库存（非线性 √ 项）；空集(D=0)时为 0
            double inventoryCost = 0.0;
            if (sumMu > 0.0) {
                inventoryCost = InventoryCostFormula::eoqCost(
                                    inst->F[j], inst->g[j], inv_factor, sumMu)
                              + InventoryCostFormula::safetyStockCost(
                                    inv_factor, inst->z_alpha, inst->L[j], sumVar);
            }

            // 新逻辑：减排投资成本 ½δw²√D（与 EOQ 同构的 √D 非线性项）
            // 仅当 use_invest_in_column=true 时纳入（此时必然用 sqrtDemandModel，
            // 因为 quadraticOnlyModel 不依赖S，无需在列利润中处理）。
            double investCost = 0.0;
            if (config->use_invest_in_column && sumMu > 0.0) {
                investCost = InvestmentCostFormula::compute(
                    *inst, j, w_j, sumMu, config->use_sqrt_investment,
                    config->investment_exponent);
            }

            // RC = columnProfit - Σλ_i - λ_DC
            //    = (sumRev - facilityCost - sumTrans - inventoryCost - investCost) - sumDual - dualDC
            double rc = constRC + sumRev - sumTrans - inventoryCost - investCost - sumDual;

            if (rc <= result.reducedCost) continue;  // 不优则无需展开/过滤

            // 展开为全 R 维（供分支约束与结果输出）
            std::fill(Sfull.begin(), Sfull.end(), 0);
            for (int i = 0; i < inst->numRetailer; i++) {
                if (fixedValue[i] == 1) Sfull[i] = 1;
            }
            for (int t = 0; t < n_neg; t++) {
                if (mask[t / kBits] & (1ULL << (t % kBits))) {
                    Sfull[negative_idx[t]] = 1;
                }
            }
            // 分支约束过滤：违反 (j,i,value) 约束则跳过
            if (branch && !branch->isColumnFeasible(j, Sfull)) continue;
            if (!RevenueFormula::serviceSetAcceptsPrice(
                    *inst, Sfull, p_j)) continue;

            result.reducedCost = rc;
            result.optimal_S = Sfull;
        }

        return result;
    }

    // ============================================================
    // 算法3(优化4)：凸包剪枝 — Andrew 单调链
    // ============================================================

    /**
     * 凸包剪枝版定价求解
     *
     * 与 Simplified 唯一区别：用 (xi, yi) 点集的凸包边界相邻点对
     * 生成半平面切割，而非 n^2 全点对。凸包边 O(n) 条，
     * 复杂度由 O(n^2) 点对降至 O(n) 点对。
     *
     * 注意：此为路径扰动启发式，相对全点对会丢内部点对连线的候选，
     * 不保证与 Simplified 结果一致，仅用于三方案对比实验。
     */
    PricingResult solveForPrice_ConvexHull(int j, double p_j,
                                             const std::vector<double>& dual,
                                             const BranchState* branch = nullptr) {
        // This geometric candidate generator was derived for the unbranched
        // free-item problem.  At B&B child nodes, required retailers may have
        // nonnegative A_i and therefore are absent from its point set.  Use the
        // branch-embedded exact candidate generator instead.
        if (branch && !branch->retailerBranches.empty()) {
            return solveForPrice_Simplified(j, p_j, dual, branch);
        }
        PricingResult result(inst->numRetailer);
        result.dcIndex = j;
        result.optimal_pj = p_j;

        double w_j = (j < static_cast<int>(inst->w.size())) ? inst->w[j] : 0.0;

        auto features = computeFeatures(j, p_j, dual);

        // 收集 Ai < 0 的零售商
        std::vector<int> negative_idx;
        std::vector<double> xi(inst->numRetailer);
        std::vector<double> yi(inst->numRetailer);

        for (int i = 0; i < inst->numRetailer; i++) {
            if (features[i].Ai < 0) {
                negative_idx.push_back(i);
                xi[i] = -static_cast<double>(features[i].mu) / features[i].Ai;
                yi[i] = -features[i].variance / features[i].Ai;
            }
        }

        int n_neg = static_cast<int>(negative_idx.size());
        std::vector<std::vector<int>> candidates;

        // 空集候选
        candidates.push_back(std::vector<int>(inst->numRetailer, 0));

        // 单元素集
        for (int t = 0; t < n_neg; t++) {
            std::vector<int> single(inst->numRetailer, 0);
            single[negative_idx[t]] = 1;
            candidates.push_back(single);
        }

        if (n_neg >= 2) {
            // ===== Andrew 单调链求凸包 =====
            // 每个点存 (x, y, 原始零售商下标)
            std::vector<std::array<double, 3>> pts;
            pts.reserve(n_neg);
            for (int t = 0; t < n_neg; t++) {
                int idx = negative_idx[t];
                pts.push_back({xi[idx], yi[idx], static_cast<double>(idx)});
            }
            std::sort(pts.begin(), pts.end(),
                      [](const std::array<double, 3>& a,
                         const std::array<double, 3>& b) {
                          if (std::abs(a[0] - b[0]) > 1e-15) return a[0] < b[0];
                          return a[1] < b[1];
                      });

            auto cross = [](const std::array<double, 3>& o,
                            const std::array<double, 3>& a,
                            const std::array<double, 3>& b) -> double {
                return (a[0] - o[0]) * (b[1] - o[1]) -
                       (a[1] - o[1]) * (b[0] - o[0]);
            };

            int m = static_cast<int>(pts.size());
            std::vector<std::array<double, 3>> hull(2 * m);
            int k = 0;
            // 下凸包
            for (int t = 0; t < m; t++) {
                while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[t]) <= 0) {
                    k--;
                }
                hull[k++] = pts[t];
            }
            // 上凸包
            for (int t = m - 2, lower = k + 1; t >= 0; t--) {
                while (k >= lower &&
                       cross(hull[k - 2], hull[k - 1], pts[t]) <= 0) {
                    k--;
                }
                hull[k++] = pts[t];
            }
            hull.resize(k > 0 ? k - 1 : 0);  // 去掉重复的起点

            // ===== 只枚举凸包边界相邻点对做半平面切割 =====
            int h = static_cast<int>(hull.size());
            for (int a = 0; a < h; a++) {
                int b = (a + 1) % h;
                int idx_m = static_cast<int>(hull[a][2]);
                int idx_n = static_cast<int>(hull[b][2]);

                double dx = xi[idx_n] - xi[idx_m];
                if (std::abs(dx) < 1e-15) continue;
                double slope = (yi[idx_n] - yi[idx_m]) / dx;
                double intercept = yi[idx_n] - slope * xi[idx_n];

                std::vector<int> below(inst->numRetailer, 0);
                std::vector<int> above(inst->numRetailer, 0);

                for (int t = 0; t < n_neg; t++) {
                    int idx_t = negative_idx[t];
                    double val = slope * xi[idx_t] + intercept;
                    if (yi[idx_t] <= val + 1e-12) below[idx_t] = 1;
                    if (yi[idx_t] >= val - 1e-12) above[idx_t] = 1;
                }

                candidates.push_back(below);
                candidates.push_back(above);
            }
        }

        // 去重并计算检验数（复用 Simplified 逻辑）
        auto vectorToKey = [](const std::vector<int>& v) -> std::string {
            std::string key;
            for (int x : v) key += (x ? '1' : '0');
            return key;
        };
        std::unordered_set<std::string> seen;
        for (const auto& S : candidates) {
            std::string key = vectorToKey(S);
            if (!seen.insert(key).second) continue;
            if (branch && !branch->isColumnFeasible(j, S)) continue;
            if (!RevenueFormula::serviceSetAcceptsPrice(
                    *inst, S, p_j)) continue;
            double rc = computeReducedCost(S, j, p_j, w_j, dual);
            if (rc > result.reducedCost) {
                result.reducedCost = rc;
                result.optimal_S = S;
            }
        }

        return result;
    }

    // ============================================================
    // 算法2：论文 Algorithm 3 — 参数线性规划凸包算法
    // ============================================================

    /**
     * 论文 Algorithm 3 实现
     *
     * 关键区别：
     * 1. 使用累积 Ḡ_j(Σμ) 和 K̄_j(Σσ²) 计算 (x, y) 坐标
     * 2. 使用余弦相似度对共线点分类 I1/I2/I3
     * 3. 对每个点对生成 12 个候选集（而非简单的上下半平面）
     *
     * 步骤：
     *   Step 1: 预处理零售商，计算 Ā_i(p_j)，收集 Ā_i < 0 的
     *   Step 2: 枚举所有点对，计算方向向量和余弦相似度
     *   Step 3: 用 F(x,y) 分区函数分类点，区分 I1/I2/I3
     *   Step 4: 生成 12 个候选集，计算完整检验数，取最优
     */
    PricingResult solveForPrice_Algorithm3(int j, double p_j,
                                            const std::vector<double>& dual,
                                            const BranchState* branch = nullptr) {
        // Algorithm 3's geometric construction assumes every item is free.
        // A right branch can force an item with A_i>=0, which never enters the
        // geometric point set.  Delegate branched prices to the formulation
        // that embeds required/forbidden retailers in all aggregate terms.
        if (branch && !branch->retailerBranches.empty()) {
            return solveForPrice_Simplified(j, p_j, dual, branch);
        }
        PricingResult result(inst->numRetailer);
        result.dcIndex = j;
        result.optimal_pj = p_j;

        double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
        auto features = computeFeatures(j, p_j, dual);

        // ===== Step 1: 收集 Ā_i < 0 的零售商，计算累积坐标 =====
        struct AlgoPoint {
            int origIdx;      // 原始零售商索引
            double e;         // Ā_i(p_j)
            double x;         // -K̄_j(累积σ²) / e
            double y;         // -Ḡ_j(累积μ) / e
            double cumMu;     // 累积需求（到该点为止的所有负Ai零售商的μ之和）
            double cumVar;    // 累积方差（到该点为止的所有负Ai零售商的σ²之和）
        };

        std::vector<AlgoPoint> points;
        double cumMu = 0.0, cumVar = 0.0;

        for (int i = 0; i < inst->numRetailer; i++) {
            if (features[i].Ai < 0) {
                cumMu += features[i].mu;
                cumVar += features[i].variance;
                AlgoPoint pt;
                pt.origIdx = features[i].idx;
                pt.e = features[i].Ai;
                pt.cumMu = cumMu;
                pt.cumVar = cumVar;
                // x = -K̄_j(cumVar) / e, y = -Ḡ_j(cumMu) / e
                double K = computeKbar(j, cumVar, w_j);
                double G = computeGbar(j, cumMu, w_j);
                pt.x = -K / pt.e;
                pt.y = -G / pt.e;
                points.push_back(pt);
            }
        }

        int num = points.size();
        if (num == 0) {
            // 无负Ai零售商：除了空集，也尝试每个零售商单独服务和完整集
            std::vector<int> bestS_candidate = std::vector<int>(inst->numRetailer, 0);
            double bestRC_candidate = computeReducedCost(bestS_candidate, j, p_j, w_j, dual);

            // 单元素集
            for (int i = 0; i < inst->numRetailer; i++) {
                std::vector<int> singleS(inst->numRetailer, 0);
                singleS[i] = 1;
                double rc = computeReducedCost(singleS, j, p_j, w_j, dual);
                if (rc > bestRC_candidate) {
                    bestRC_candidate = rc;
                    bestS_candidate = singleS;
                }
            }

            // 完整集（所有零售商）
            std::vector<int> allS(inst->numRetailer, 1);
            double rcAll = computeReducedCost(allS, j, p_j, w_j, dual);
            if (rcAll > bestRC_candidate) {
                bestRC_candidate = rcAll;
                bestS_candidate = allS;
            }

            result.reducedCost = bestRC_candidate;
            result.optimal_S = bestS_candidate;
            return result;
        }

        // 最佳候选集
        std::vector<int> bestS(inst->numRetailer, 0);
        double bestRC = computeReducedCost(bestS, j, p_j, w_j, dual);

        // ===== 枚举所有候选集并评估 =====
        // 收集所有候选集索引（points 索引，非原始零售商索引）
        std::vector<std::vector<int>> candidateSets = { {} };

        // 放入所有单元素集和完整集
        for (int t = 0; t < num; t++) {
            candidateSets.push_back({t});
        }
        {
            std::vector<int> all;
            for (int t = 0; t < num; t++) all.push_back(t);
            candidateSets.push_back(all);
        }

        // ===== Step 2-3: 枚举所有点对 =====
        for (int p = 0; p < num - 1; p++) {
            for (int q = p + 1; q < num; q++) {
                double xp = points[p].x, yp = points[p].y;
                double xq = points[q].x, yq = points[q].y;

                // 跳过相同点
                if (std::abs(xp - xq) < 1e-15 && std::abs(yp - yq) < 1e-15) continue;

                // 计算余弦相似度 cos(p,q)
                double dot = xp * xq + yp * yq;
                double normP = std::sqrt(xp * xp + yp * yp);
                double normQ = std::sqrt(xq * xq + yq * yq);
                double cosPQ = (normP > 1e-15 && normQ > 1e-15) ? dot / (normP * normQ) : 0.0;

                // 定义分区函数 F(x,y)
                auto F = [&](double x, double y) -> double {
                    if (std::abs(xp - xq) > 1e-15 && std::abs(yp - yq) > 1e-15) {
                        return (y - yp) / (yq - yp) - (x - xp) / (xq - xp);
                    } else if (std::abs(xp - xq) > 1e-15) {
                        return x - xp;
                    } else {
                        return y - yp;
                    }
                };

                // ===== Step 3: 分类点 =====
                std::vector<int> Rp, Rq;   // 两侧的点（原始零售商索引）
                std::vector<int> I1, I2, I3; // 共线点分类

                for (int t = 0; t < num; t++) {
                    if (t == p || t == q) continue;

                    double val = F(points[t].x, points[t].y);

                    if (val > 1e-12) {
                        Rp.push_back(t);
                    } else if (val < -1e-12) {
                        Rq.push_back(t);
                    } else {
                        // 共线点：用余弦相似度进一步分类
                        // cos(pt) 和 cos(qt)
                        double xpt = points[t].x - xp, ypt = points[t].y - yp;
                        double xqt = points[t].x - xq, yqt = points[t].y - yq;
                        double normPt = std::sqrt(xpt*xpt + ypt*ypt);
                        double normQt = std::sqrt(xqt*xqt + yqt*yqt);

                        if (normPt > 1e-15) {
                            double cosPQ_PT = (xpt*(xq-xp) + ypt*(yq-yp)) / (normPt * std::sqrt((xq-xp)*(xq-xp)+(yq-yp)*(yq-yp)));
                            if (cosPQ_PT < -0.999999) {
                                I1.push_back(t);  // 相反方向共线
                                continue;
                            }
                        }
                        if (normQt > 1e-15) {
                            double cosQP_QT = (xqt*(xp-xq) + yqt*(yp-yq)) / (normQt * std::sqrt((xp-xq)*(xp-xq)+(yp-yq)*(yp-yq)));
                            if (cosQP_QT > 0.999999) {
                                I2.push_back(t);  // 相同方向共线
                                continue;
                            }
                        }
                        I3.push_back(t);  // 其他边界情况
                    }
                }

                // ===== Step 4: 枚举 12 个候选集 =====
                // 从 Rp 侧出发的 5 个候选
                std::vector<std::vector<int>> candidatesFromRp = {
                    Rp,                                    // 1. Rp
                    concat(Rp, I1),                        // 2. Rp ∪ I1
                    concat(Rp, I1, {p}),                   // 3. Rp ∪ I1 ∪ {p}
                    concat(Rp, I1, {p}, I2),               // 4. Rp ∪ I1 ∪ {p} ∪ I2
                    concat(Rp, I1, {p}, I2, {q}),          // 5. Rp ∪ I1 ∪ {p} ∪ I2 ∪ {q}
                };
                // 从 Rq 侧出发的 5 个候选
                std::vector<std::vector<int>> candidatesFromRq = {
                    Rq,                                    // 6. Rq
                    concat(Rq, I1),                        // 7. Rq ∪ I1
                    concat(Rq, I1, {p}),                   // 8. Rq ∪ I1 ∪ {p}
                    concat(Rq, I1, {p}, I2),               // 9. Rq ∪ I1 ∪ {p} ∪ I2
                    concat(Rq, I1, {p}, I2, {q}),          // 10. Rq ∪ I1 ∪ {p} ∪ I2 ∪ {q}
                };
                // 额外：仅 {p} 和 {q}（从论文算法中推断，完整覆盖）
                std::vector<std::vector<int>> extraCandidates = {
                    {p},                                    // 11. {p}
                    {q},                                    // 12. {q}
                };

                // 把 Algorithm 3 的 12 个候选集加入总候选列表
                for (const auto& c : candidatesFromRp) candidateSets.push_back(c);
                for (const auto& c : candidatesFromRq) candidateSets.push_back(c);
                for (const auto& c : extraCandidates) candidateSets.push_back(c);
            }
        }

        // 加入简化凸包枚举的候选集（用个体坐标 (xi,yi)=(-μ/Ai, -σ²/Ai)）
        // 确保 algorithm3 不遗漏简化版能找到的候选
        {
            for (int m = 0; m < num - 1; m++) {
                for (int n = m + 1; n < num; n++) {
                    double xi_m = -features[points[m].origIdx].mu / points[m].e;
                    double yi_m = -features[points[m].origIdx].variance / points[m].e;
                    double xi_n = -features[points[n].origIdx].mu / points[n].e;
                    double yi_n = -features[points[n].origIdx].variance / points[n].e;
                    double dx = xi_n - xi_m;
                    if (std::abs(dx) < 1e-15) continue;
                    double k = (yi_n - yi_m) / dx;
                    double b = yi_n - k * xi_n;
                    std::vector<int> below, above;
                    for (int t = 0; t < num; t++) {
                        double xi_t = -features[points[t].origIdx].mu / points[t].e;
                        double yi_t = -features[points[t].origIdx].variance / points[t].e;
                        double val = k * xi_t + b;
                        if (yi_t <= val + 1e-12) below.push_back(t);
                        if (yi_t >= val - 1e-12) above.push_back(t);
                    }
                    candidateSets.push_back(below);
                    candidateSets.push_back(above);
                }
            }
        }

        // 去重并对所有候选集计算检验数，取最优
        auto setToKey = [](const std::vector<int>& indices) -> std::string {
            std::string key;
            for (int idx : indices) key += std::to_string(idx) + ",";
            return key;
        };
        std::unordered_set<std::string> seen;
        for (const auto& selIndices : candidateSets) {
            std::string key = setToKey(selIndices);
            if (!seen.insert(key).second) continue;

            // 把 points 索引映射回原始零售商索引，构建 S 向量
            std::vector<int> S(inst->numRetailer, 0);
            for (int ptIdx : selIndices) {
                S[points[ptIdx].origIdx] = 1;
            }
            // 分支约束过滤：跳过违反 (j,i,value) 约束的服务集合
            if (branch && !branch->isColumnFeasible(j, S)) continue;
            if (!RevenueFormula::serviceSetAcceptsPrice(
                    *inst, S, p_j)) continue;
            double rc = computeReducedCost(S, j, p_j, w_j, dual);
            if (rc > bestRC) {
                bestRC = rc;
                bestS = S;
            }
        }

        result.reducedCost = bestRC;
        result.optimal_S = bestS;
        return result;
    }

    /**
     * 辅助函数：连接多个 vector<int>（用于 Step 4 的候选集构建）
     */
    static std::vector<int> concat(const std::vector<int>& a, const std::vector<int>& b) {
        std::vector<int> r = a;
        r.insert(r.end(), b.begin(), b.end());
        // 去重（同一元素可能出现在 I1 和 Rp 中）
        std::sort(r.begin(), r.end());
        r.erase(std::unique(r.begin(), r.end()), r.end());
        return r;
    }

    static std::vector<int> concat(const std::vector<int>& a, const std::vector<int>& b,
                                    const std::vector<int>& c) {
        std::vector<int> r = concat(a, b);
        r.insert(r.end(), c.begin(), c.end());
        std::sort(r.begin(), r.end());
        r.erase(std::unique(r.begin(), r.end()), r.end());
        return r;
    }

    static std::vector<int> concat(const std::vector<int>& a, const std::vector<int>& b,
                                    const std::vector<int>& c, const std::vector<int>& d) {
        std::vector<int> r = concat(a, b, c);
        r.insert(r.end(), d.begin(), d.end());
        std::sort(r.begin(), r.end());
        r.erase(std::unique(r.begin(), r.end()), r.end());
        return r;
    }

    static std::vector<int> concat(const std::vector<int>& a, const std::vector<int>& b,
                                    const std::vector<int>& c, const std::vector<int>& d,
                                    const std::vector<int>& e) {
        std::vector<int> r = concat(a, b, c, d);
        r.insert(r.end(), e.begin(), e.end());
        std::sort(r.begin(), r.end());
        r.erase(std::unique(r.begin(), r.end()), r.end());
        return r;
    }
};

#endif // PRICING_SOLVER_H
