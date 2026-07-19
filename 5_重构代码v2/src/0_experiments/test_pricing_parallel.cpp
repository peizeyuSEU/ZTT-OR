/**
 * 定价子问题并行效率测试程序
 *
 * 测试目的：
 *   单独测量 PricingSolver 的求解效率，验证定价子问题的并行加速比。
 *   定价子问题是纯 C++ 枚举算法（不依赖 CPLEX），理论上应有接近线性的加速比。
 *
 * 测试方法：
 *   1. 用默认参数生成数据
 *   2. 生成 dummy 对偶变量
 *   3. 分别用串行和不同线程数并行求解所有 DC 的定价子问题
 *   4. 统计时间对比
 *
 * 编译：g++ -std=c++17 -O2 -I src test_pricing_parallel.cpp -o ../bin/test_pricing
 * 运行：./bin/test_pricing
 */

#include "../10_common/Instance.h"
#include "../10_common/Config.h"
#include "../10_common/Types.h"
#include "../6_preprocessor/DataGenerator.h"
#include "../8_solver/01_PricingSolver.h"
#include "../10_common/ThreadPool.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <future>
#include <thread>
#include <numeric>
#include <algorithm>

void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

struct TestResult {
    std::string label;
    int numThreads;
    double totalTime;
    double avgTimePerDC;
    double speedup;       // vs 串行
    double efficiency;    // speedup / numThreads
};

int main() {
    std::cout << std::fixed << std::setprecision(4);
    int maxCores = std::thread::hardware_concurrency();
    std::cout << "CPU核心数: " << maxCores << std::endl;

    // ===== 1. 测试数据规模配置 =====
    struct TestCase {
        std::string name;
        int numDC;
        int numRetailers;
    };

    std::vector<TestCase> testCases = {
        {"3 DC × 10 零售商",  3,  10},
        {"5 DC × 20 零售商",  5,  20},
        {"10 DC × 30 零售商", 10, 30},
        {"20 DC × 50 零售商", 20, 50},
        {"30 DC × 100 零售商", 30, 100},
    };

    // ===== 2. 线程数配置 =====
    std::vector<int> threadCounts = {1};
    for (int t = 2; t <= maxCores; t *= 2) {
        threadCounts.push_back(t);
    }
    if (threadCounts.back() < maxCores) {
        threadCounts.push_back(maxCores);
    }

    // 每个测试重复次数
    const int NUM_REPEATS = 3;

    // ===== 3. 对每个测试规模执行 =====
    for (const auto& tc : testCases) {
        printHeader("测试规模: " + tc.name + " (重复" + std::to_string(NUM_REPEATS) + "次取中位)");

        // 3a. 生成测试数据
        Config cfg;
        cfg.num_dc = tc.numDC;
        cfg.num_retailers = tc.numRetailers;
        cfg.random_seed = 32501;
        cfg.delta = 3000;
        cfg.holding_cost = 10.0;
        cfg.use_sqrt_investment = true;

        DataGenerator gen(cfg.random_seed);
        Instance inst = gen.generate(cfg);

        // 3b. 生成 dummy 对偶变量
        std::vector<double> dual(inst.numRetailer + tc.numDC, 0.0);
        for (int i = 0; i < inst.numRetailer; i++) {
            dual[i] = 10.0 + (i % 5) * 3.0;
        }
        for (int j = 0; j < tc.numDC; j++) {
            dual[inst.numRetailer + j] = 5.0 + j * 2.0;
        }

        // 3c. 对每个线程数测试
        std::vector<TestResult> results;

        for (int numThreads : threadCounts) {
            if (numThreads > tc.numDC) continue;

            std::vector<double> times;

            for (int rep = 0; rep < NUM_REPEATS; rep++) {
                auto start = std::chrono::steady_clock::now();

                if (numThreads == 1) {
                    // 串行求解所有DC
                    PricingSolver serSolver;
                    serSolver.setInstance(inst);
                    serSolver.setConfig(cfg);
                    serSolver.setRCEps(1e-6);
                    for (int j = 0; j < tc.numDC; j++) {
                        serSolver.solve(j, dual);
                    }
                } else {
                    // 并行求解
                    ThreadPool pool(numThreads);
                    std::vector<std::future<PricingResult>> futures(tc.numDC);
                    for (int j = 0; j < tc.numDC; j++) {
                        futures[j] = pool.enqueue([&inst, &cfg, j, &dual]() {
                            PricingSolver localSolver;
                            localSolver.setInstance(inst);
                            localSolver.setConfig(cfg);
                            localSolver.setRCEps(1e-6);
                            return localSolver.solve(j, dual);
                        });
                    }
                    for (int j = 0; j < tc.numDC; j++) {
                        futures[j].get();
                    }
                }

                auto end = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(end - start).count();
                times.push_back(elapsed);
            }

            // 取中位数
            std::sort(times.begin(), times.end());
            double medianTime = times[times.size() / 2];

            TestResult tr;
            tr.label = (numThreads == 1) ? "串行(1线程)" : std::to_string(numThreads) + "线程";
            tr.numThreads = numThreads;
            tr.totalTime = medianTime;
            tr.avgTimePerDC = medianTime / tc.numDC;
            tr.speedup = 0.0;
            tr.efficiency = 0.0;
            results.push_back(tr);
        }

        // 3d. 计算加速比
        double serialTime = results[0].totalTime;
        for (auto& r : results) {
            if (r.numThreads == 1) {
                r.speedup = 1.0;
                r.efficiency = 1.0;
            } else {
                r.speedup = serialTime / r.totalTime;
                r.efficiency = r.speedup / r.numThreads;
            }
        }

        // 3e. 输出表格
        std::cout << std::left << std::setw(18) << "模式"
                  << std::right << std::setw(12) << "总时间(s)"
                  << std::setw(14) << "平均/DC(ms)"
                  << std::setw(12) << "加速比"
                  << std::setw(14) << "效率" << std::endl;
        std::cout << std::string(18 + 12 + 14 + 12 + 14, '-') << std::endl;

        for (const auto& r : results) {
            std::cout << std::left << std::setw(18) << r.label
                      << std::right << std::setw(12) << r.totalTime
                      << std::setw(14) << (r.avgTimePerDC * 1000)
                      << std::setw(12) << r.speedup
                      << std::setw(14) << (r.efficiency * 100)
                      << "%" << std::endl;
        }

        // 3f. 额外：Algorithm3 模式（只在最小规模）
        if (tc.numDC <= 5 && tc.numRetailers <= 20) {
            std::cout << "\n  --- Algorithm3 模式 (use_paper_algorithm3=true) ---" << std::endl;

            Config cfg2 = cfg;
            cfg2.use_paper_algorithm3 = true;
            DataGenerator gen3(cfg2.random_seed);
            Instance inst3 = gen3.generate(cfg2);

            // 串行
            PricingSolver serSolver;
            serSolver.setInstance(inst3);
            serSolver.setConfig(cfg2);
            serSolver.setRCEps(1e-6);

            auto sStart = std::chrono::steady_clock::now();
            for (int j = 0; j < tc.numDC; j++) {
                serSolver.solve(j, dual);
            }
            auto sEnd = std::chrono::steady_clock::now();
            double serTime = std::chrono::duration<double>(sEnd - sStart).count();

            // 并行
            int bestT = std::min(tc.numDC, maxCores);
            ThreadPool pool(bestT);
            std::vector<std::future<PricingResult>> futures(tc.numDC);
            auto pStart = std::chrono::steady_clock::now();
            for (int j = 0; j < tc.numDC; j++) {
                futures[j] = pool.enqueue([&inst3, &cfg2, j, &dual]() {
                    PricingSolver localSolver;
                    localSolver.setInstance(inst3);
                    localSolver.setConfig(cfg2);
                    localSolver.setRCEps(1e-6);
                    return localSolver.solve(j, dual);
                });
            }
            for (int j = 0; j < tc.numDC; j++) {
                futures[j].get();
            }
            auto pEnd = std::chrono::steady_clock::now();
            double parTime = std::chrono::duration<double>(pEnd - pStart).count();

            double speedup = serTime / parTime;
            double eff = speedup / bestT * 100;
            std::cout << "    串行: " << serTime << "s, 并行(" << bestT << "线程): "
                      << parTime << "s, 加速比: " << speedup
                      << ", 效率: " << eff << "%" << std::endl;
        }
    }

    // ===== 4. 总结 =====
    printHeader("总结：定价子问题并行效率");
    std::cout << "  - 定价子问题是纯 C++ 枚举算法（不依赖 CPLEX 求解器）" << std::endl;
    std::cout << "  - 理论上应具有良好的并行扩展性" << std::endl;
    std::cout << "  - 加速比受限于：线程池开销、内存带宽竞争" << std::endl;
    std::cout << "  - 注意：当前 parallel_pricing 只在列生成内部使用" << std::endl;
    std::cout << "    （max 4线程），且 GA 的 parallel_fitness 才是更大粒度的并行" << std::endl;

    return 0;
}
