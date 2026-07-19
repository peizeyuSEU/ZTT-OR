#ifndef SOLUTION_H
#define SOLUTION_H

#include "Types.h"
#include "Instance.h"
#include <iostream>
#include <iomanip>
#include <fstream>

class SolutionHelper {
public:
    static void printResult(const std::string& label, const Solution& sol,
                            double solveTime, double noInvestProfit,
                            double noInvestEmission, double carbonEmission) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "===== " << label << " =====" << std::endl;
        std::cout << "总利润: " << sol.totalProfit << std::endl;
        std::cout << "碳排放: " << carbonEmission << std::endl;
        std::cout << "求解时间(秒): " << solveTime << std::endl;
        std::cout << "#DCs Open: " << sol.numDCsOpen << std::endl;
        std::cout << "#Rts Served: " << sol.numRtsServed << std::endl;
        std::cout << "不减排投资总利润: " << noInvestProfit << std::endl;
        std::cout << "不减排投资碳排放: " << noInvestEmission << std::endl;
        std::cout << "==========================" << std::endl;
    }

    static void printSensitivityResult(const std::string& label,
                                       const Solution& sol,
                                       double solveTime, double delta,
                                       double carbonEmission) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "===== δ = " << delta << " =====" << std::endl;
        std::cout << "总利润: " << sol.totalProfit << std::endl;
        std::cout << "碳排放: " << carbonEmission << std::endl;
        std::cout << "减排投资成本: " << sol.totalInvestmentCost << std::endl;
        std::cout << "求解时间(秒): " << solveTime << std::endl;
        std::cout << "#DCs Open: " << sol.numDCsOpen << std::endl;
        std::cout << "#Rts Served: " << sol.numRtsServed << std::endl;
        std::cout << "w: {";
        for (size_t i = 0; i < sol.w.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << sol.w[i];
        }
        std::cout << "}" << std::endl;
        std::cout << "==========================" << std::endl;
    }

    static void printConvergenceCSV(const std::vector<ConvergencePoint>& points) {
        std::cout << "Generation,BestFitness,AvgFitness,WorstFitness,ElapsedTime" << std::endl;
        for (const auto& p : points) {
            std::cout << p.generation << ","
                      << p.bestFitness << ","
                      << p.avgFitness << ","
                      << p.worstFitness << ","
                      << p.elapsedTime << std::endl;
        }
    }

    static void saveConvergenceCSV(const std::string& filename,
                                   const std::vector<ConvergencePoint>& points) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[Solution] 无法写入 " << filename << std::endl;
            return;
        }
        file << "Generation,BestFitness,AvgFitness,WorstFitness,ElapsedTime" << std::endl;
        for (const auto& p : points) {
            file << p.generation << ","
                 << p.bestFitness << ","
                 << p.avgFitness << ","
                 << p.worstFitness << ","
                 << p.elapsedTime << std::endl;
        }
        file.close();
    }
};

#endif
