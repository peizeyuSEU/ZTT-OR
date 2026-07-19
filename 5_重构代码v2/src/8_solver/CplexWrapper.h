#ifndef CPLEX_WRAPPER_H
#define CPLEX_WRAPPER_H

#include <ilcplex/ilocplex.h>
#include <vector>
#include <iostream>
#include <string>
#include <stdexcept>
#include <cmath>

ILOSTLBEGIN

/** CPLEX 环境封装（RAII） */
class CplexEnv {
private:
    IloEnv env;
    bool verbose = false;

public:
    CplexEnv() = default;

    ~CplexEnv() {
        try {
            env.end();
        } catch (...) {
        }
    }

    /** 获取底层 IloEnv */
    operator IloEnv() { return env; }

    IloEnv& get() { return env; }
    const IloEnv& get() const { return env; }

    void setVerbose(bool v) { verbose = v; }

    /**
     * 创建 IloCplex 求解器并配置
     */
    IloCplex createSolver(const IloModel& model) {
        IloCplex cplex(model);
        if (!verbose) {
            cplex.setOut(env.getNullStream());
        }
        cplex.setParam(IloCplex::Threads, 1);
        cplex.setParam(IloCplex::EpOpt, 1e-9);
        return cplex;
    }

    /**
     * 求解并检查结果
     */
    static bool solve(IloCplex& cplex, const std::string& context = "") {
        try {
            if (!cplex.solve()) {
                if (!context.empty()) {
                    std::cerr << "[CPLEX] " << context << " 求解失败" << std::endl;
                }
                return false;
            }
            return true;
        } catch (const IloException& e) {
            std::cerr << "[CPLEX] " << context << " 异常: " << e.getMessage() << std::endl;
            return false;
        }
    }

    /** 检查值是否为整数（容差内） */
    static bool isIntegerValue(double val, double eps = 1e-6) {
        double rounded = std::round(val);
        return std::abs(val - rounded) < eps;
    }
};

/**
 * 快速构建限制主问题（RMP）的辅助函数
 * 注意：IloModel 对象必须提前创建好，传入引用
 */
inline void buildMasterProblem(
    IloEnv env,
    IloModel& model,
    IloObjective& totalProfit,
    IloRangeArray& Fill,
    IloArray<IloNumVarArray>& z_j,
    int numDC,
    int numRetailer,
    const std::vector<std::vector<double>>& profits,
    const std::vector<std::vector<std::vector<int>>>& j_S,
    const std::vector<int>& branchId,
    const std::vector<int>& lb,
    const std::vector<int>& ub,
    int addSize
) {
    totalProfit = IloAdd(model, IloMaximize(env));

    int numConstraints = numRetailer + numDC;
    IloNumArray up(env, numConstraints);
    for (int k = 0; k < numConstraints; k++) {
        up[k] = 1;
    }
    Fill = IloAdd(model, IloRangeArray(env, -IloInfinity, up));

    for (int j = 0; j < numDC; j++) {
        IloNumVarArray z_j_s(env);
        for (int m = 0; m < (int)profits[j].size(); m++) {
            IloNumColumn col = totalProfit(profits[j][m]);
            for (int n = 0; n < numRetailer; n++) {
                if (j_S[j][m][n] == 1) {
                    col = col + Fill[n](1);
                }
            }
            int kk = numRetailer + j;
            col = col + Fill[kk](1);
            z_j_s.add(IloNumVar(col, 0, 1, ILOFLOAT));
        }
        z_j.add(z_j_s);
    }

    for (int i = 0; i < addSize; i++) {
        int dcIdx = branchId[i * 2];
        int colIdx = branchId[i * 2 + 1];
        if (dcIdx < z_j.getSize() && colIdx < z_j[dcIdx].getSize()) {
            z_j[dcIdx][colIdx].setBounds(lb[i], ub[i]);
        }
    }
}

inline void addColumnToModel(
    IloObjective& totalProfit,
    IloRangeArray& Fill,
    IloArray<IloNumVarArray>& z_j,
    int optimal_j,
    const std::vector<int>& optimal_S,
    double profit_value,
    int numRetailer
) {
    IloNumColumn col = totalProfit(profit_value);
    for (int n = 0; n < numRetailer; n++) {
        if (optimal_S[n] == 1) {
            col = col + Fill[n](1);
        }
    }
    int j_index = numRetailer + optimal_j;
    col = col + Fill[j_index](1);
    z_j[optimal_j].add(IloNumVar(col, 0, 1, ILOFLOAT));
}

#endif // CPLEX_WRAPPER_H
