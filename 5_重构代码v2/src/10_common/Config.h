#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

/**
 * 配置管理类
 *
 * 从一个 YAML 文件读取所有参数，同时支持：
 * 1. 全局默认参数（键值对）
 * 2. 实验列表（experiments: 下的一串列表，每个条目可独立覆盖任意字段）
 *
 * 用法：
 *   Config cfg;
 *   cfg.loadFromFile("experiments/table4_1.yaml");
 *
 *   // 单次运行：直接用 cfg 的默认值
 *
 *   // 批量运行：遍历 experiments
 *   for (auto& entry : cfg.experiments) {
 *       Config runCfg = cfg;
 *       runCfg.applyEntry(entry);  // 用 entry 的 overrides 覆盖
 *       ExperimentRunner::run(runCfg, logger);
 *   }
 */

// ===== 实验条目 =====
struct ExperimentEntry {
    std::string name;                        // 实验名称
    std::map<std::string, std::string> overrides;  // 要覆盖的字段
};

class Config {
public:
    // ========== 问题规模 ==========
    int num_dc = 3;
    int num_retailers = 10;

    // ========== 遗传算法参数 ==========
    int population_size = 30;
    int max_generation = 50;
    double crossover_rate = 0.7;
    double mutation_rate = 0.2;
    bool elitism = true;
    int chromosome_length = 10;
    double min_w = 0.0;
    double max_w = 1.0;

    // ========== 碳交易参数 ==========
    double carbon_price = 2.0;           // p_hat
    double carbon_cap = 100000.0;        // C
    double inv_carbon_coeff = 2.0;       // h_hat
    double transport_carbon_coeff = 0.01;// k
    double fac_carbon_ratio = 0.1;       // f_hat_j = f_j * fac_carbon_ratio

    // ========== 减排参数 ==========
    double delta = 5000.0;

    // ========== 服务水平 ==========
    double service_level = 0.975;
    double z_alpha = 1.96;

    // ========== 其他固定参数 ==========
    double order_fixed_cost = 100.0;     // F_j
    double transport_fixed_cost = 100.0; // g_j
    double lead_time = 1.0;              // L_j
    double holding_cost = 10.0;          // h（论文表5-1：10）

    // ========== 数据生成参数 ==========
    int random_seed = 32501;
    double coord_min = 0.0;
    double coord_max = 1000.0;
    double mu_min = 2000.0;
    double mu_max = 3500.0;
    double var_min = 10.0;
    double var_max = 15.0;
    double fixed_cost_min = 7000.0;
    double fixed_cost_max = 10000.0;
    double reserve_price_min = 60.0;
    double reserve_price_max = 80.0;
    double supplier_x = 500.0;
    double supplier_y = 500.0;

    // ========== 输出控制 ==========
    bool verbose = false;
    std::string output_dir = "results";
    std::string log_file = "";

    // ========== 分支定界参数 ==========
    double rc_eps = 1.0e-6;
    int max_branch_nodes = 10000;
    int max_cg_iterations = 2000;  // 列生成单次求解的最大迭代次数（防数值抖动死循环）
    bool use_dfs = true;

    // ========== 列生成 tailing-off 早停参数 ==========
    // 列生成尾部常出现"每轮只加微小改善列"的拖尾现象（tailing-off）。
    // 当 RMP 目标值连续 cg_stall_iterations 轮相对改善均 < cg_stall_tolerance 时，
    // 视为已实质收敛并提前终止，避免上万轮无效迭代。
    bool cg_early_stop = true;            // 是否启用 tailing-off 早停
    int cg_stall_iterations = 20;         // 连续 N 轮改善不足则停
    double cg_stall_tolerance = 1.0e-6;   // 目标值相对改善阈值

    // ========== 方案A：对偶价格平滑（Wentges stabilization） ==========
    // 喂给定价的对偶 = alpha * 上一轮平滑对偶 + (1 - alpha) * 本轮 LP 对偶。
    // alpha=0 关闭平滑（退化为原始直接用对偶）；只影响列生成路径与收敛速度，不改变 LP 最优值。
    //
    // 【默认关闭(0.0)的实测依据】平滑的前提是"对偶来回震荡"，但本问题实测根节点
    //   对偶/obj 一路单调平稳上升、根本不震荡，此时平滑纯属负担：它让定价子问题
    //   反复生成同一批列结构，导致列池提前"顶上界假收敛"停在次优 LP，再被迫关平滑
    //   用真实对偶重新探索，等于把列生成做了两遍。铁证(同解，比轮数)：
    //     30x100_w00: 开=77轮 vs 关=56轮；20x70_w05: 开=57 vs 关=44；30x100_w05: 开=78 vs 关=60。
    //   三格无论退化(w=0)还是健康(w=0.5)均是"关平滑"更快 → 默认关闭最优。
    //   （若某算例确有对偶震荡，可在配置里手动置 alpha>0；届时 02_ColumnGeneration.h
    //     的假收敛平滑保护会兜底防止 cycling，不会死循环。）
    double dual_smooth_alpha = 0.0;       // 对偶平滑系数 [0,1)，0=关闭（默认关闭，见上）

    // ========== 方案B：根节点整数启发式（抬 bestLowerBound 加速剪枝） ==========
    // 根节点 LP 分数解后，用 rounding/贪心构造一个可行整数解作为初始下界，
    // 使分支定界的剪枝尽早生效。只抬下界，不影响最终全局最优性。
    bool root_heuristic = true;           // 是否启用根节点整数启发式

    // ========== 公式模型选择 ==========
    bool use_sqrt_investment = true;     // true=½δw²√D, false=½βw²（论文标准版）

    // ========== 定价子问题算法选择 ==========
    bool use_paper_algorithm3 = false;   // true=论文Algorithm3（完整凸包+余弦相似度）
                                         // false=简化凸包枚举（当前算法，更快）
                                         // 注：保留此字段仅为向后兼容，实际调度以 pricing_algorithm 为准

    // 三方案对比开关：0=Simplified(简化枚举) / 1=Algorithm3(论文精确) / 2=ConvexHull(凸包剪枝)
    int pricing_algorithm = 0;

    // 每个 DC 每轮定价返回的最多列数（multi-column pricing）。
    // 默认 1 = 只返回最优 1 列（历史行为，零改变）。
    // 设为 >1 时，每个 DC 每轮额外返回缓存池中当前对偶下的多个正检验数列，
    // 一次性向 RMP 注入更多列以压缩大规模列生成的迭代轮数（解不变，仅减轮数）。
    int pricing_max_cols_per_dc = 1;

    // ========== 并行计算参数 ==========
    bool parallel_fitness = false;        // 是否并行评估GA个体
    int num_threads = 0;                  // 线程数（0=自动检测CPU核心数）
    bool parallel_pricing = false;        // 是否并行求解定价子问题
    // RMP 主问题的 CPLEX 求解线程数（1=单线程，安全默认）。
    // 注意：当 parallel_fitness=true(GA fork 并行) 时会被强制回退为 1，
    // 因为 fork 已占满 CPU 核，CPLEX 再开多线程将过度订阅导致死锁。
    int cplex_threads = 1;

    // 定价子问题并行的线程数上限（每个 BP 实例内部）。默认 4，保持历史行为。
    // 实际线程数 = min(numDC, pricing_threads, 预算约束)，下限 1。
    int pricing_threads = 4;
    // 总核预算：约束 GA 并发进程数 × 定价线程数 <= core_budget，防止吃光服务器核。
    // 0 = 不限制（退化为历史行为：定价线程 = min(numDC, pricing_threads)）。
    int core_budget = 0;

    // ========== 提前停止参数 ==========
    bool early_stop = false;              // 是否提前停止GA
    int convergence_generations = 10;     // 连续N代不提升则停止
    double convergence_tolerance = 0.001; // 提升阈值（相对值）

    // ========== 运行模式 ==========
    std::string run_mode = "ga";         // "ga"=完整GA+BP, "fixed_w"=固定w只跑BP
    std::vector<double> fixed_w;         // run_mode="fixed_w"时使用的w值

    // ========== 实验模式 ==========
    bool legacy_mode = false;            // 使用原代码 GA_BP.cpp 的数据生成方式
    bool transport_direct_distance = false; // 运输成本直接用距离（原版代码方式）

    // ========== 实验列表（从 YAML 的 experiments: 段读取）==========
    std::vector<ExperimentEntry> experiments;
    std::string plan_name;               // 方案名称
    std::string plan_desc;               // 方案说明

public:
    Config() = default;

    /**
     * 从YAML文件加载配置
     *
     * 支持格式：
     *   key: value            ← 键值对
     *   key: "string value"   ← 字符串值
     *   experiments:          ← 实验列表
     *     - name: "xxx"
     *       num_dc: 3
     *       ...
     */
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[Config] 错误: 无法打开配置文件 " << filename << std::endl;
            return false;
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        file.close();

        // 当前解析上下文
        bool inExperiments = false;       // 是否在 experiments: 块中
        ExperimentEntry currentEntry;     // 正在解析的实验条目
        bool inEntry = false;             // 是否在某个实验条目的内部
        int entryIndent = -1;             // 实验条目的缩进级别

        for (size_t lineno = 0; lineno < lines.size(); lineno++) {
            std::string raw = lines[lineno];
            std::string trimmed = trim(raw);
            if (trimmed.empty()) continue;
            if (trimmed[0] == '#') continue;

            // 计算缩进（空格数）
            int indent = 0;
            for (char c : raw) {
                if (c == ' ') indent++;
                else break;
            }

            // ===== 检测 experiments: 段开始 =====
            if (trimmed == "experiments:" || startsWith(trimmed, "experiments:")) {
                inExperiments = true;
                continue;
            }

            if (!inExperiments) {
                // ===== 全局键值对 =====
                size_t colon = trimmed.find(':');
                if (colon == std::string::npos) continue;

                std::string key = trim(trimmed.substr(0, colon));
                std::string value = trim(trimmed.substr(colon + 1));
                if (key.empty()) continue;

                // 跳过空值（可能是 section 标题如 "experiments:" 已被上面的逻辑捕获）
                if (value.empty()) continue;

                setField(key, value);
            } else {
                // ===== experiments: 块内 =====

                // 检测列表项开始：- name: xxx
                if (startsWith(trimmed, "- ")) {
                    // 保存上一个条目
                    if (inEntry && !currentEntry.name.empty()) {
                        experiments.push_back(currentEntry);
                    }

                    currentEntry = ExperimentEntry();
                    inEntry = true;
                    entryIndent = indent;

                    // 解析 "- name: xxx"
                    std::string afterDash = trim(trimmed.substr(2));
                    size_t colon = afterDash.find(':');
                    if (colon != std::string::npos) {
                        std::string key = trim(afterDash.substr(0, colon));
                        std::string value = trim(afterDash.substr(colon + 1));
                        if (key == "name") {
                            currentEntry.name = removeQuotes(value);
                        } else {
                            currentEntry.overrides[key] = value;
                        }
                    }
                } else if (inEntry && indent > entryIndent) {
                    // 条目内部的键值对（缩进比 - 更深）
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        std::string key = trim(trimmed.substr(0, colon));
                        std::string value = trim(trimmed.substr(colon + 1));
                        if (!key.empty() && !value.empty()) {
                            if (key == "name") {
                                currentEntry.name = removeQuotes(value);
                            } else {
                                currentEntry.overrides[key] = value;
                            }
                        }
                    }
                } else if (indent <= entryIndent + 1 && indent > 0 && !startsWith(trimmed, "-")) {
                    // experiments 块下的下一级键值对（可能是新的 section 开始，忽略）
                    // 但需要判断是否是 experiments 段结束
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        std::string key = trim(trimmed.substr(0, colon));
                        std::string value = trim(trimmed.substr(colon + 1));
                        if (!value.empty()) {
                            // 还在 experiments 块内的键值对？当做全局
                            setField(key, value);
                        }
                    }
                } else if (indent == 0 && !startsWith(trimmed, "-")) {
                    // experiments 段结束，回到全局
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        std::string key = trim(trimmed.substr(0, colon));
                        std::string value = trim(trimmed.substr(colon + 1));
                        if (!value.empty()) {
                            inExperiments = false;
                            setField(key, value);
                        }
                    }
                }
            }
        }

        // 保存最后一个条目
        if (inEntry && !currentEntry.name.empty()) {
            experiments.push_back(currentEntry);
        }

        return true;
    }

    /**
     * 将 ExperimentEntry 的 overrides 应用到当前 Config
     * 用于批量实验中，对每个条目生成一份独立的配置
     */
    void applyEntry(const ExperimentEntry& entry) {
        for (const auto& [key, value] : entry.overrides) {
            setField(key, value);
        }
    }

    /** 打印当前配置 */
    void print() const {
        std::cout << "========== 配置参数 ==========" << std::endl;
        std::cout << "问题规模: #DC=" << num_dc << ", #零售商=" << num_retailers << std::endl;
        std::cout << "遗传算法: 种群=" << population_size
                  << ", 代数=" << max_generation
                  << ", 交叉率=" << crossover_rate
                  << ", 变异率=" << mutation_rate
                  << ", 编码位数=" << chromosome_length << std::endl;
        std::cout << "碳交易: 碳价=" << carbon_price
                  << ", 配额=" << carbon_cap << std::endl;
        std::cout << "减排参数: δ=" << delta << std::endl;
        std::cout << "随机种子: " << random_seed << std::endl;
        std::cout << "实验条目数: " << experiments.size() << std::endl;
        if (!experiments.empty()) {
            std::cout << "实验列表: ";
            for (const auto& e : experiments) {
                std::cout << e.name << " ";
            }
            std::cout << std::endl;
        }
        std::cout << "================================" << std::endl;
    }

    /**
     * 保存当前配置为YAML文件
     */
    void saveToFile(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[Config] 无法写入 " << filename << std::endl;
            return;
        }

        file << "# 配置参数（自动生成）\n";
        saveField(file, "num_dc", num_dc);
        saveField(file, "num_retailers", num_retailers);
        saveField(file, "population_size", population_size);
        saveField(file, "max_generation", max_generation);
        saveField(file, "crossover_rate", crossover_rate);
        saveField(file, "mutation_rate", mutation_rate);
        saveField(file, "elitism", elitism);
        saveField(file, "chromosome_length", chromosome_length);
        saveField(file, "carbon_price", carbon_price);
        saveField(file, "carbon_cap", carbon_cap);
        saveField(file, "delta", delta);
        saveField(file, "random_seed", random_seed);
        file.close();
    }

    /**
     * 将当前 Config 序列化为 YAML 格式的字符串
     * 用于 run.cpp 中打印单次实验的完整配置
     */
    std::string toYamlString() const {
        std::stringstream ss;
        ss << "num_dc: " << num_dc << "\n";
        ss << "num_retailers: " << num_retailers << "\n";
        ss << "population_size: " << population_size << "\n";
        ss << "max_generation: " << max_generation << "\n";
        ss << "crossover_rate: " << crossover_rate << "\n";
        ss << "mutation_rate: " << mutation_rate << "\n";
        ss << "elitism: " << (elitism ? "true" : "false") << "\n";
        ss << "chromosome_length: " << chromosome_length << "\n";
        ss << "carbon_price: " << carbon_price << "\n";
        ss << "carbon_cap: " << carbon_cap << "\n";
        ss << "delta: " << delta << "\n";
        ss << "random_seed: " << random_seed << "\n";
        ss << "holding_cost: " << holding_cost << "\n";
        return ss.str();
    }

private:
    // ========== 工具方法 ==========

    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    static bool startsWith(const std::string& str, const std::string& prefix) {
        return str.size() >= prefix.size()
            && str.substr(0, prefix.size()) == prefix;
    }

    static std::string removeQuotes(const std::string& str) {
        std::string s = trim(str);
        if (s.size() >= 2 && s[0] == '"' && s.back() == '"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }

    /**
     * 从值字符串中去除行内注释（从第一个未引用的 # 开始截断）
     * 例如 "3000.0     # 减排系数" → "3000.0"
     * 空字符串或全注释字符串返回空串
     */
    static std::string stripInlineComment(const std::string& str) {
        std::string s = trim(str);
        // 如果是被引号包裹的完整字符串，不去除内部 #（如 "" 或 "a#b" 中的 #）
        if (s.size() >= 2 && s[0] == '"' && s.back() == '"') {
            return s;
        }
        size_t pos = s.find('#');
        if (pos != std::string::npos) {
            s = s.substr(0, pos);
            s = trim(s);
        }
        return s;
    }

    static bool toBool(const std::string& raw_value) {
        std::string v = trim(stripInlineComment(raw_value));
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        return v == "true" || v == "yes" || v == "1";
    }

    // ========== 字段设置（所有可配置字段的注册表）==========

    void setField(const std::string& key, const std::string& raw_value) {
        // 去除行内注释
        std::string value = stripInlineComment(raw_value);
        if (key == "num_dc") num_dc = std::stoi(value);
        else if (key == "num_retailers") num_retailers = std::stoi(value);
        else if (key == "population_size") population_size = std::stoi(value);
        else if (key == "max_generation") max_generation = std::stoi(value);
        else if (key == "crossover_rate") crossover_rate = std::stod(value);
        else if (key == "mutation_rate") mutation_rate = std::stod(value);
        else if (key == "elitism") elitism = toBool(value);
        else if (key == "chromosome_length") chromosome_length = std::stoi(value);
        else if (key == "min_w") min_w = std::stod(value);
        else if (key == "max_w") max_w = std::stod(value);
        else if (key == "carbon_price") carbon_price = std::stod(value);
        else if (key == "carbon_cap") carbon_cap = std::stod(value);
        else if (key == "inv_carbon_coeff") inv_carbon_coeff = std::stod(value);
        else if (key == "transport_carbon_coeff") transport_carbon_coeff = std::stod(value);
        else if (key == "fac_carbon_ratio") fac_carbon_ratio = std::stod(value);
        else if (key == "delta") delta = std::stod(value);
        else if (key == "service_level") service_level = std::stod(value);
        else if (key == "z_alpha") z_alpha = std::stod(value);
        else if (key == "order_fixed_cost") order_fixed_cost = std::stod(value);
        else if (key == "transport_fixed_cost") transport_fixed_cost = std::stod(value);
        else if (key == "lead_time") lead_time = std::stod(value);
        else if (key == "holding_cost") holding_cost = std::stod(value);
        else if (key == "random_seed") random_seed = std::stoi(value);
        else if (key == "coord_min") coord_min = std::stod(value);
        else if (key == "coord_max") coord_max = std::stod(value);
        else if (key == "mu_min") mu_min = std::stod(value);
        else if (key == "mu_max") mu_max = std::stod(value);
        else if (key == "var_min") var_min = std::stod(value);
        else if (key == "var_max") var_max = std::stod(value);
        else if (key == "fixed_cost_min") fixed_cost_min = std::stod(value);
        else if (key == "fixed_cost_max") fixed_cost_max = std::stod(value);
        else if (key == "reserve_price_min") reserve_price_min = std::stod(value);
        else if (key == "reserve_price_max") reserve_price_max = std::stod(value);
        else if (key == "supplier_x") supplier_x = std::stod(value);
        else if (key == "supplier_y") supplier_y = std::stod(value);
        else if (key == "verbose") verbose = toBool(value);
        else if (key == "output_dir") output_dir = value;
        else if (key == "log_file") log_file = value;
        else if (key == "rc_eps") rc_eps = std::stod(value);
        else if (key == "max_branch_nodes") max_branch_nodes = std::stoi(value);
     else if (key == "max_cg_iterations") max_cg_iterations = std::stoi(value);
        else if (key == "cg_early_stop") cg_early_stop = toBool(value);
        else if (key == "cg_stall_iterations") cg_stall_iterations = std::stoi(value);
        else if (key == "cg_stall_tolerance") cg_stall_tolerance = std::stod(value);
        else if (key == "dual_smooth_alpha") dual_smooth_alpha = std::stod(value);
        else if (key == "root_heuristic") root_heuristic = toBool(value);
        else if (key == "use_dfs") use_dfs = toBool(value);
        else if (key == "use_sqrt_investment") use_sqrt_investment = toBool(value);
        else if (key == "use_paper_algorithm3") {
            use_paper_algorithm3 = toBool(value);
            // 向后兼容：旧配置只设 use_paper_algorithm3 时，同步映射到 pricing_algorithm
            pricing_algorithm = use_paper_algorithm3 ? 1 : 0;
        }
        else if (key == "pricing_algorithm") {
            pricing_algorithm = std::stoi(value);
            // 反向同步布尔字段，便于旧代码路径读取
            use_paper_algorithm3 = (pricing_algorithm == 1);
        }
        else if (key == "pricing_max_cols_per_dc") pricing_max_cols_per_dc = std::stoi(value);
        else if (key == "parallel_fitness") parallel_fitness = toBool(value);
        else if (key == "num_threads") num_threads = std::stoi(value);
        else if (key == "parallel_pricing") parallel_pricing = toBool(value);
        else if (key == "cplex_threads") cplex_threads = std::stoi(value);
        else if (key == "pricing_threads") pricing_threads = std::stoi(value);
        else if (key == "core_budget") core_budget = std::stoi(value);
        else if (key == "early_stop") early_stop = toBool(value);
        else if (key == "convergence_generations") convergence_generations = std::stoi(value);
        else if (key == "convergence_tolerance") convergence_tolerance = std::stod(value);
        else if (key == "legacy_mode") legacy_mode = toBool(value);
        else if (key == "transport_direct_distance") transport_direct_distance = toBool(value);
        else if (key == "run_mode") run_mode = value;
        else if (key == "fixed_w") {
            // 解析形如 "[0, 0, 0]" 的列表
            std::string v = value;
            if (!v.empty() && v.front() == '[') v = v.substr(1);
            if (!v.empty() && v.back() == ']') v = v.substr(0, v.size() - 1);
            std::stringstream ss(v);
            std::string token;
            while (std::getline(ss, token, ',')) {
                fixed_w.push_back(std::stod(trim(token)));
            }
        }
        // 忽略未知字段（允许实验条目中有注释性字段）
    }

    static void saveField(std::ofstream& file, const std::string& name, int value) {
        file << name << ": " << value << "\n";
    }

    static void saveField(std::ofstream& file, const std::string& name, double value) {
        file << name << ": " << value << "\n";
    }

    static void saveField(std::ofstream& file, const std::string& name, bool value) {
        file << name << ": " << (value ? "true" : "false") << "\n";
    }
};

#endif // CONFIG_H
