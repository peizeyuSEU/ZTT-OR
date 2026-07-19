#ifndef PRICING_SOLVER_H
#define PRICING_SOLVER_H

#include "../0_base/01_types/Instance.h"
#include "../0_base/01_types/Types.h"
#include "../2_config/Config.h"
#include "../3_formula/03_TransportCost.h"
#include "../3_formula/04_InventoryCost.h"
#include "../3_formula/08_Objective.h"
#include "../0_base/03_utils/Logger.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <chrono>
#include <unordered_set>
#include <string>

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

public:
    PricingSolver() : inst(nullptr), config(nullptr), logger(nullptr),
                      rc_eps(1e-6), pricingTime(0.0) {}

    void setInstance(const Instance& instance) { inst = &instance; }
    void setConfig(const Config& cfg) { config = &cfg; }
    void setRCEps(double eps) { rc_eps = eps; }
    void setLogger(Logger* log) { logger = log; }

    double getPricingTime() const { return pricingTime; }

    /**
     * 求解DC j的定价子问题
     *
     * @param j DC索引
     * @param dual 对偶变量数组（长度 = 零售商数 + DC数）
     * @return 最优列（S, p_j）和检验数
     */
    PricingResult solve(int j, const std::vector<double>& dual) {
        auto start = std::chrono::steady_clock::now();

        PricingResult result(inst->numRetailer);
        result.dcIndex = j;
        result.reducedCost = -DBL_MAX;

        // 候选价格为各零售商的保留价
        std::vector<double> candidate_prices;
        for (int i = 0; i < inst->numRetailer; i++) {
            candidate_prices.push_back(inst->reservePrice[i]);
        }
        // 去重
        std::sort(candidate_prices.begin(), candidate_prices.end());
        candidate_prices.erase(
            std::unique(candidate_prices.begin(), candidate_prices.end()),
            candidate_prices.end()
        );

        // 对每个候选价格求解最优服务集合，取RC最大的
        bool usePaperAlgo = config ? config->use_paper_algorithm3 : false;
        for (double price : candidate_prices) {
            PricingResult subResult;
            if (usePaperAlgo) {
                subResult = solveForPrice_Algorithm3(j, price, dual);
            } else {
                subResult = solveForPrice_Simplified(j, price, dual);
            }
            if (subResult.reducedCost > result.reducedCost) {
                result = subResult;
            }
        }

        auto end = std::chrono::steady_clock::now();
        pricingTime += std::chrono::duration<double>(end - start).count();

        return result;
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
     */
    std::vector<RetailerFeature> computeFeatures(int j, double p_j,
                                                   const std::vector<double>& dual) const {
        double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;
        std::vector<RetailerFeature> features;

        for (int i = 0; i < inst->numRetailer; i++) {
            double new_revenue;
            if (p_j > inst->reservePrice[i]) {
                new_revenue = 0.0;
            } else {
                new_revenue = p_j;
            }

            double trans_cost_dc_retail = TransportCostFormula::costDCToRetailer(inst->dist[i][j])
                + inst->p * (1.0 - w_j) * inst->bb[i][j];
            double trans_cost_sup_dc = TransportCostFormula::costSupplierToDC(inst->a_dist[j])
                + inst->p * (1.0 - w_j) * inst->b[j];

            double Ai = (trans_cost_dc_retail + trans_cost_sup_dc - new_revenue)
                        * inst->mu[i] + dual[i];

            RetailerFeature f;
            f.idx = i;
            f.Ai = Ai;
            f.mu = inst->mu[i];
            f.variance = inst->variance[i];
            features.push_back(f);
        }
        return features;
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
        double invest_part = 0.5 * inst->delta * w_j * w_j * std::sqrt(totalDemand);
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
        ObjectiveFormula objFormula;
        double profit = objFormula.columnProfit(*inst, j, S, p_j, w_j);
        double rc = profit;
        for (int i = 0; i < inst->numRetailer; i++) {
            if (S[i] == 1) rc -= dual[i];
        }
        rc -= dual[inst->numRetailer + j];
        return rc;
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
     * 简化凸包枚举算法
     *
     * 使用 (xi, yi) = (-μ_i/Ai, -σ²_i/Ai)
     * 枚举所有两点组合的上下半平面，生成候选集
     * 对所有候选计算完整检验数，取最优
     */
    PricingResult solveForPrice_Simplified(int j, double p_j,
                                            const std::vector<double>& dual) {
        PricingResult result(inst->numRetailer);
        result.dcIndex = j;
        result.optimal_pj = p_j;

        double w_j = (j < (int)inst->w.size()) ? inst->w[j] : 0.0;

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

        int n_neg = negative_idx.size();
        std::vector<std::vector<int>> candidates;

        // 空集候选
        candidates.push_back(std::vector<int>(inst->numRetailer, 0));

        if (n_neg == 1) {
            std::vector<int> one(inst->numRetailer, 0);
            one[negative_idx[0]] = 1;
            candidates.push_back(one);
        } else if (n_neg >= 2) {
            // 单元素集
            for (int t = 0; t < n_neg; t++) {
                std::vector<int> single(inst->numRetailer, 0);
                single[negative_idx[t]] = 1;
                candidates.push_back(single);
            }

            // 枚举所有两点组合
            for (int m = 0; m < n_neg - 1; m++) {
                for (int n = m + 1; n < n_neg; n++) {
                    int idx_m = negative_idx[m];
                    int idx_n = negative_idx[n];

                    double dx = xi[idx_n] - xi[idx_m];
                    if (std::abs(dx) < 1e-15) continue;
                    double k = (yi[idx_n] - yi[idx_m]) / dx;
                    double b = yi[idx_n] - k * xi[idx_n];

                    std::vector<int> below(inst->numRetailer, 0);
                    std::vector<int> above(inst->numRetailer, 0);

                    for (int t = 0; t < n_neg; t++) {
                        int idx_t = negative_idx[t];
                        double val = k * xi[idx_t] + b;
                        if (yi[idx_t] <= val + 1e-12) below[idx_t] = 1;
                        if (yi[idx_t] >= val - 1e-12) above[idx_t] = 1;
                    }

                    candidates.push_back(below);
                    candidates.push_back(above);
                }
            }
        }

        // 去重并计算检验数
        auto vectorToKey = [](const std::vector<int>& v) -> std::string {
            std::string key;
            for (int x : v) key += (x ? '1' : '0');
            return key;
        };
        std::unordered_set<std::string> seen;
        for (const auto& S : candidates) {
            std::string key = vectorToKey(S);
            if (!seen.insert(key).second) continue;
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
                                            const std::vector<double>& dual) {
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
