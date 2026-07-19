#ifndef METRICS_H
#define METRICS_H

#include "../01_types/Types.h"
#include "../01_types/Instance.h"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>

/**
 * 性能指标收集和统计
 */
class Metrics {
public:
    // 分支定界统计
    int totalBranchNodes = 0;          // 总分支节点数
    int prunedNodes = 0;               // 被剪枝的节点数
    int integerSolutions = 0;          // 找到的整数解数
    int columnGenIterations = 0;       // 列生成总迭代次数
    int totalColumnsGenerated = 0;     // 总生成的列数

    // 时间统计
    double columnGenTime = 0.0;        // 列生成耗时
    double pricingTime = 0.0;          // 定价子问题耗时
    double branchTime = 0.0;           // 分支定界耗时
    double totalTime = 0.0;            // 总耗时

    // 收敛曲线
    std::vector<ConvergencePoint> convergence;

    Metrics() = default;

    /** 记录一次列生成迭代 */
    void recordColumnIteration() {
        columnGenIterations++;
    }

    /** 记录生成的新列 */
    void recordNewColumn(int count = 1) {
        totalColumnsGenerated += count;
    }

    /** 添加收敛数据点 */
    void addConvergencePoint(int gen, double best, double avg,
                              double worst, double elapsed) {
        convergence.push_back({gen, best, avg, worst, elapsed});
    }

    /** 保存收敛曲线到CSV */
    void saveConvergence(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[Metrics] 无法写入 " << filename << std::endl;
            return;
        }
        file << "Generation,BestFitness,AvgFitness,WorstFitness,ElapsedTime" << std::endl;
        for (const auto& p : convergence) {
            file << p.generation << ","
                 << std::fixed << std::setprecision(6)
                 << p.bestFitness << ","
                 << p.avgFitness << ","
                 << p.worstFitness << ","
                 << p.elapsedTime << std::endl;
        }
        file.close();
    }

    /** 打印统计摘要 */
    void printSummary() const {
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "===== 求解统计 =====" << std::endl;
        std::cout << "总分支节点数: " << totalBranchNodes << std::endl;
        std::cout << "剪枝节点数: " << prunedNodes << std::endl;
        std::cout << "整数解数: " << integerSolutions << std::endl;
        std::cout << "列生成迭代次数: " << columnGenIterations << std::endl;
        std::cout << "生成列总数: " << totalColumnsGenerated << std::endl;
        std::cout << "列生成耗时: " << columnGenTime << "s" << std::endl;
        std::cout << "定价子问题耗时: " << pricingTime << "s" << std::endl;
        std::cout << "分支定界耗时: " << branchTime << "s" << std::endl;
        std::cout << "=========================" << std::endl;
    }
};

#endif // METRICS_H
