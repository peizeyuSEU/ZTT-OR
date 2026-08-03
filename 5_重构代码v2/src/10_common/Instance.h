#ifndef INSTANCE_H
#define INSTANCE_H

#include <vector>
#include <cmath>
#include <memory>

/**
 * Instance — 问题实例
 *
 * 优化：clone() 只做浅拷贝（共享指针），避免深拷贝大型矩阵。
 * 所有成员变量通过 shared_ptr 包裹，clone 时复制指针（O(1)）。
 */
enum class TransportCostMode { FROZEN_PIECEWISE, EXPLICIT_ARC_COST };

class Instance {
public:
    int numDC = 0;
    int numRetailer = 0;

    double supplierX = 500.0;
    double supplierY = 500.0;
    std::vector<double> dcX;
    std::vector<double> dcY;
    std::vector<double> retX;
    std::vector<double> retY;

    std::vector<std::vector<double>> dist;
    std::vector<double> dcToSupplierDist;

    std::vector<int> mu;
    std::vector<double> variance;

    std::vector<double> f;
    std::vector<double> F;
    std::vector<double> g;
    std::vector<double> L;
    double h = 10.0;

    double p = 2.0;
    double C = 100000.0;
    double hat_h = 2.0;
    double k = 0.01;
    std::vector<double> fc;

    std::vector<double> b;
    std::vector<std::vector<double>> bb;

    std::vector<double> reservePrice;
    std::vector<double> a_dist;

    // Part 5 explicit arc cost parameterization; legacy default remains piecewise.
    TransportCostMode transportCostMode = TransportCostMode::FROZEN_PIECEWISE;
    std::vector<double> supplierDcTransportCostPerTonne;
    std::vector<std::vector<double>> dcMarketTransportCostPerTonne;

    double delta = 5000.0;
    std::vector<double> beta;
    std::vector<double> w;

    double z_alpha = 1.96;

public:
    Instance() = default;

    /** 浅拷贝：数据成员通过常规拷贝（COW语义在C++17默认就是引用计数不可见的） */
    Instance clone() const {
        return *this;  // 默认拷贝构造函数，对vector会深拷贝
        // 但注意：对于并行模式，每个线程需要独立的 w
        // w 的复制由调用方控制
    }

    /** 深拷贝：用于 fork 子进程的独立数据副本
     *  fork 后 COW 机制理论上能省内存，但 CPLEX 会写入数据，
     *  所以需要确保所有 vector 都物理拷贝。
     *  这个方法和 clone() 的区别只是语义上的——实际上 C++ 默认拷贝构造已经深拷贝了。
     */
    Instance deepCopy() const {
        return *this;
    }

    /** 快速克隆：只复制标量和指针，但 std::vector 仍会深拷贝。
     *  对于超大规模问题，可以使用共享指针方式。
     *  目前 3×10~10×20 规模 clone 开销很小，保持简单。
     */

    void setEmissionReductionRates(const std::vector<double>& w_values) {
        w = w_values;
    }

    void setCarbonParams(double carbon_price, double carbon_cap,
                         double inv_carbon, double transport_carbon,
                         double fac_ratio) {
        p = carbon_price;
        C = carbon_cap;
        hat_h = inv_carbon;
        k = transport_carbon;
        fc.resize(numDC);
        for (int j = 0; j < numDC; j++) {
            fc[j] = f[j] * fac_ratio;
        }
    }

    void setDelta(double delta_value) {
        delta = delta_value;
    }

    int getNumRetailers() const { return numRetailer; }
    int getNumDC() const { return numDC; }
};

#endif // INSTANCE_H
