/**
 * 第0层：实验层 — 批量运行实验
 *
 * 读一个 YAML 实验方案文件，遍历 experiments 列表，逐条运行。
 *
 * YAML 文件结构：
 *   plan_name: "方案名称"
 *   plan_desc: "方案说明"
 *   # 全局默认参数（所有 Config 支持的字段）
 *   num_dc: 3
 *   population_size: 30
 *   ...
 *   experiments:
 *     - name: "实验1"
 *       num_dc: 3
 *       num_retailers: 5
 *       # 其他字段继承全局默认值
 *     - name: "实验2"
 *       num_dc: 3
 *       num_retailers: 10
 *       delta: 1000
 *
 * 用法：
 *   ./bin/run                            # 默认跑 experiments/table4_1.yaml
 *   ./bin/run experiments/verify.yaml    # 指定实验方案
 *
 * 注意：run 可执行文件在 bin/ 下，实验方案路径相对于运行时的当前目录。
 *       建议在 4_重构代码/ 目录下运行：
 *         cd 4_重构代码
 *         ./bin/run
 *         ./bin/run experiments/table4_2.yaml
 */

#include "../2_config/Config.h"
#include "../0_base/03_utils/Logger.h"
#include "../1_launcher/ExperimentRunner.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>

static void printTableHeader(std::ostream& out) {
    out << std::left
        << std::setw(16) << "名称"
        << std::setw(6) << "#DC"
        << std::setw(6) << "#Rts"
        << std::setw(16) << "总利润"
        << std::setw(14) << "碳排放"
        << std::setw(10) << "时间(s)"
        << std::setw(10) << "#DC开"
        << std::setw(10) << "#R服务"
        << std::setw(16) << "基准利润"
        << std::setw(10) << "节点数" << std::endl;
    out << std::string(120, '-') << std::endl;
}

static void printTableRow(std::ostream& out, const std::string& name,
                           const ExperimentResult& r) {
    out << std::left << std::fixed << std::setprecision(2);
    out << std::setw(16) << name
        << std::setw(6) << r.numDC
        << std::setw(6) << r.numRetailers
        << std::setw(16) << r.totalProfit
        << std::setw(14) << r.carbonEmission
        << std::setw(10) << r.solveTime
        << std::setw(10) << r.numDCsOpen
        << std::setw(10) << r.numRtsServed
        << std::setw(16) << r.baseProfit
        << std::setw(10) << r.branchNodes << std::endl;
}

int main(int argc, char** argv) {
    // 默认实验方案
    std::string planFile = "src/0_experiments/table4_1.yaml";
    if (argc >= 2) planFile = argv[1];

    // 加载实验方案
    Config planConfig;
    if (!planConfig.loadFromFile(planFile)) {
        std::cerr << "[实验层] 无法加载实验方案: " << planFile << std::endl;
        return 1;
    }

    if (planConfig.experiments.empty()) {
        std::cerr << "[实验层] 实验方案中没有定义 experiments 列表: " << planFile << std::endl;
        return 1;
    }

    // 创建结果目录
    mkdir("results", 0755);

    std::cout << "\n========== 批量实验开始 ==========\n" << std::endl;
    std::cout << "方案文件: " << planFile << std::endl;
    std::cout << "方案名称: " << planConfig.plan_name << std::endl;
    std::cout << "方案说明: " << planConfig.plan_desc << std::endl;
    std::cout << "共 " << planConfig.experiments.size() << " 组实验" << std::endl;
    std::cout << "默认种子: " << planConfig.random_seed << std::endl;
    std::cout << std::endl;

    std::ofstream resultFile("results/experiments.txt");
    resultFile << "===== 批量实验结果 =====" << std::endl;
    resultFile << "方案: " << planFile << std::endl;
    resultFile << "说明: " << planConfig.plan_desc << std::endl;
    resultFile << "\n";

    printTableHeader(std::cout);
    printTableHeader(resultFile);

    Logger logger;

    // 遍历实验列表
    for (const auto& entry : planConfig.experiments) {
        // 从 planConfig 复制一份，再用 entry 的 overrides 覆盖
        Config runConfig = planConfig;
        runConfig.applyEntry(entry);

        std::string label = entry.name;

        std::cout << "\n运行: " << label
                  << " (#DC=" << runConfig.num_dc
                  << ", #Rts=" << runConfig.num_retailers
                  << ", δ=" << runConfig.delta
                  << ", seed=" << runConfig.random_seed << ")" << std::endl;

        auto start = std::chrono::steady_clock::now();
        ExperimentResult r = ExperimentRunner::run(runConfig, logger);
        auto end = std::chrono::steady_clock::now();
        r.solveTime = std::chrono::duration<double>(end - start).count();

        printTableRow(std::cout, label, r);
        printTableRow(resultFile, label, r);
    }

    resultFile.close();

    std::cout << "\n结果已保存至 results/experiments.txt" << std::endl;
    std::cout << "========== 批量实验完成 ==========" << std::endl;
    std::cout << "提示: 如需更改参数，直接编辑 " << planFile << std::endl;
    std::cout << "      每个实验条目可独立覆盖任意配置字段。" << std::endl;

    return 0;
}
