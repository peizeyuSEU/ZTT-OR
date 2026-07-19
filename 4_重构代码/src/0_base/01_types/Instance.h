#ifndef INSTANCE_H
#define INSTANCE_H

#include <vector>
#include <cmath>

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

    double delta = 5000.0;
    std::vector<double> beta;
    std::vector<double> w;

    double z_alpha = 1.96;

public:
    Instance() = default;

    /** 深度复制 */
    Instance clone() const {
        Instance c;
        c.numDC = numDC;
        c.numRetailer = numRetailer;
        c.supplierX = supplierX;
        c.supplierY = supplierY;
        c.dcX = dcX;
        c.dcY = dcY;
        c.retX = retX;
        c.retY = retY;
        c.dist = dist;
        c.dcToSupplierDist = dcToSupplierDist;
        c.mu = mu;
        c.variance = variance;
        c.f = f;
        c.F = F;
        c.g = g;
        c.L = L;
        c.h = h;
        c.p = p;
        c.C = C;
        c.hat_h = hat_h;
        c.k = k;
        c.fc = fc;
        c.b = b;
        c.bb = bb;
        c.reservePrice = reservePrice;
        c.a_dist = a_dist;
        c.delta = delta;
        c.beta = beta;
        c.w = w;
        c.z_alpha = z_alpha;
        return c;
    }

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

#endif
