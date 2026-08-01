#ifndef DATA_GENERATOR_H
#define DATA_GENERATOR_H

#include "../10_common/Instance.h"
#include "../10_common/Random.h"
#include "../10_common/Config.h"
#include <cmath>
#include <iostream>

class DataGenerator {
private:
    Random rng;
    bool legacyMode = false;

    long seed1 = 32501, seed2 = 51501, seed3 = 13501;
    long seed4 = 66501, seed5 = 28501, seed6 = 80501;
    long seed7 = 2745, seed8 = 79973, seed9 = 166645, seed10 = 28894, seed11 = 68823, seed99 = 8623;

    static long legacyRand(long& s) {
        s = (s * 1103515245 + 12345) & 0x7fffffff;
        return s;
    }

    static double legacyUnif(long& s, int jingdu2) {
        return (double)(legacyRand(s) % (jingdu2 + 1)) / (double)(jingdu2 + 1);
    }

public:
    explicit DataGenerator(uint32_t seed) : rng(seed) {}

    void setLegacyMode(bool mode) { legacyMode = mode; }
    bool isLegacyMode() const { return legacyMode; }

    Instance generate(const Config& config) {
        if (legacyMode) {
            return generateLegacy(config);
        }
        return generateStandard(config);
    }

    Instance generateStandard(const Config& config) {
        Instance inst;
        inst.numDC = config.num_dc;
        inst.numRetailer = config.num_retailers;
        inst.supplierX = config.supplier_x;
        inst.supplierY = config.supplier_y;
        const double coordMin = config.coord_min;
        const double coordMax = config.coord_max;

        inst.dcX.resize(inst.numDC);
        inst.dcY.resize(inst.numDC);
        for (int j = 0; j < inst.numDC; j++) {
            inst.dcX[j] = rng.uniform(coordMin, coordMax);
            inst.dcY[j] = rng.uniform(coordMin, coordMax);
        }

        inst.retX.resize(inst.numRetailer);
        inst.retY.resize(inst.numRetailer);
        for (int i = 0; i < inst.numRetailer; i++) {
            inst.retX[i] = rng.uniform(coordMin, coordMax);
            inst.retY[i] = rng.uniform(coordMin, coordMax);
        }

        inst.dist.resize(inst.numRetailer, std::vector<double>(inst.numDC));
        for (int i = 0; i < inst.numRetailer; i++) {
            for (int j = 0; j < inst.numDC; j++) {
                double dx = inst.dcX[j] - inst.retX[i];
                double dy = inst.dcY[j] - inst.retY[i];
                inst.dist[i][j] = std::sqrt(dx * dx + dy * dy);
            }
        }

        inst.a_dist.resize(inst.numDC);
        inst.dcToSupplierDist.resize(inst.numDC);
        for (int j = 0; j < inst.numDC; j++) {
            double dx = inst.dcX[j] - inst.supplierX;
            double dy = inst.dcY[j] - inst.supplierY;
            double dist = std::sqrt(dx * dx + dy * dy);
            inst.a_dist[j] = dist;
            inst.dcToSupplierDist[j] = dist;
        }

        inst.mu.resize(inst.numRetailer);
        inst.variance.resize(inst.numRetailer);
        for (int i = 0; i < inst.numRetailer; i++) {
            inst.mu[i] = static_cast<int>(rng.uniform(config.mu_min, config.mu_max));
            inst.variance[i] = rng.uniform(config.var_min, config.var_max);
        }

        inst.f.resize(inst.numDC);
        inst.F.resize(inst.numDC);
        inst.g.resize(inst.numDC);
        inst.L.resize(inst.numDC);
        for (int j = 0; j < inst.numDC; j++) {
            inst.f[j] = rng.uniform(config.fixed_cost_min, config.fixed_cost_max);
            inst.F[j] = config.order_fixed_cost;
            inst.g[j] = config.transport_fixed_cost;
            inst.L[j] = config.lead_time;
        }
        inst.h = config.holding_cost;

        inst.p = config.carbon_price;
        inst.C = config.carbon_cap;
        inst.hat_h = config.inv_carbon_coeff;
        inst.k = config.transport_carbon_coeff;

        inst.fc.resize(inst.numDC);
        for (int j = 0; j < inst.numDC; j++) {
            inst.fc[j] = inst.f[j] * config.fac_carbon_ratio;
        }

        inst.b.resize(inst.numDC);
        inst.bb.resize(inst.numRetailer, std::vector<double>(inst.numDC));
        for (int j = 0; j < inst.numDC; j++) {
            inst.b[j] = inst.k * inst.a_dist[j];
            for (int i = 0; i < inst.numRetailer; i++) {
                inst.bb[i][j] = inst.k * inst.dist[i][j];
            }
        }

        inst.reservePrice.resize(inst.numRetailer);
        for (int i = 0; i < inst.numRetailer; i++) {
            inst.reservePrice[i] = rng.uniform(config.reserve_price_min, config.reserve_price_max);
        }

        inst.delta = config.delta;
        inst.beta.resize(inst.numDC);
        for (int j = 0; j < inst.numDC; j++) {
            inst.beta[j] = config.delta;
        }
        inst.w.resize(inst.numDC, 0.0);
        inst.z_alpha = config.z_alpha;

        return inst;
    }

    Instance generateLegacy(const Config& config) {
        Instance inst;
        inst.numDC = config.num_dc;
        inst.numRetailer = config.num_retailers;
        inst.supplierX = 50.0;
        inst.supplierY = 50.0;

        int jingdu2 = 99;

        seed1 = 32501; seed2 = 51501; seed3 = 13501;
        seed4 = 66501; seed5 = 28501; seed6 = 80501;
        seed7 = 2745; seed8 = 79973; seed9 = 166645; seed10 = 28894; seed11 = 68823; seed99 = 8623;

        inst.dcX.resize(inst.numDC);
        inst.dcY.resize(inst.numDC);
        inst.f.resize(inst.numDC);
        inst.F.resize(inst.numDC);
        inst.g.resize(inst.numDC);
        inst.L.resize(inst.numDC);
        inst.fc.resize(inst.numDC);
        inst.a_dist.resize(inst.numDC);
        inst.dcToSupplierDist.resize(inst.numDC);
        inst.b.resize(inst.numDC);
        inst.beta.resize(inst.numDC);
        inst.w.resize(inst.numDC, 0.0);
        inst.retX.resize(inst.numRetailer);
        inst.retY.resize(inst.numRetailer);
        inst.mu.resize(inst.numRetailer);
        inst.variance.resize(inst.numRetailer);
        inst.reservePrice.resize(inst.numRetailer);
        inst.dist.resize(inst.numRetailer, std::vector<double>(inst.numDC));
        inst.bb.resize(inst.numRetailer, std::vector<double>(inst.numDC));

        for (int j = 0; j < inst.numDC; j++) {
            inst.dcX[j] = legacyUnif(seed1, jingdu2) * 100;
            inst.dcY[j] = legacyUnif(seed2, jingdu2) * 100;
            inst.f[j] = legacyUnif(seed3, jingdu2) * 10 + 20;
            inst.L[j] = 1;
            inst.F[j] = 10;
            inst.g[j] = legacyUnif(seed99, jingdu2) * 500 + 300;
            double dist = std::sqrt(
                (inst.dcX[j] - 50) * (inst.dcX[j] - 50) +
                (inst.dcY[j] - 50) * (inst.dcY[j] - 50));
            inst.a_dist[j] = dist;
            inst.dcToSupplierDist[j] = dist;
            inst.fc[j] = inst.f[j] / 2;
            inst.b[j] = 300;
            inst.beta[j] = legacyUnif(seed11, jingdu2) * 100000 + 100000;

            for (int i = 0; i < inst.numRetailer; i++) {
                if (j == 0) {
                    inst.reservePrice[i] = legacyUnif(seed8, jingdu2) * 300 + 400;
                    inst.mu[i] = (int)(legacyUnif(seed4, jingdu2) * 3 + 2) * 100;
                    inst.variance[i] = legacyUnif(seed9, jingdu2) * 9 + 10;
                    inst.retX[i] = legacyUnif(seed5, jingdu2) * 100;
                    inst.retY[i] = legacyUnif(seed6, jingdu2) * 100;
                }
            }
        }

        for (int j = 0; j < inst.numDC; j++) {
            for (int i = 0; i < inst.numRetailer; i++) {
                inst.dist[i][j] = std::sqrt(
                    (inst.dcX[j] - inst.retX[i]) * (inst.dcX[j] - inst.retX[i]) +
                    (inst.dcY[j] - inst.retY[i]) * (inst.dcY[j] - inst.retY[i]));
                inst.bb[i][j] = inst.dist[i][j] / 2;
            }
        }

        inst.p = 1.0;
        inst.C = 40000.0;
        inst.h = 1.0;
        inst.hat_h = 2.0;
        inst.z_alpha = 1.96;
        inst.k = 0.01;
        inst.delta = config.delta;

        return inst;
    }

    void resetSeed(uint32_t seed) {
        rng.setSeed(seed);
        seed1 = 32501; seed2 = 51501; seed3 = 13501;
        seed4 = 66501; seed5 = 28501; seed6 = 80501;
        seed7 = 2745; seed8 = 79973; seed9 = 166645; seed10 = 28894; seed11 = 68823; seed99 = 8623;
    }
};

#endif
