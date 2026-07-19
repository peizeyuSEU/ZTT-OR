#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include "../01_types/Instance.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

class DataLoader {
public:
    static Instance loadFromCSV(const std::string& filename) {
        Instance inst;
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[DataLoader] 无法打开 " << filename << std::endl;
            return inst;
        }

        std::string line;
        if (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ','); inst.numDC = std::stoi(token);
            std::getline(ss, token, ','); inst.numRetailer = std::stoi(token);
        }

        inst.dcX.resize(inst.numDC);
        inst.dcY.resize(inst.numDC);
        inst.f.resize(inst.numDC);
        inst.F.resize(inst.numDC);
        inst.g.resize(inst.numDC);
        inst.L.resize(inst.numDC);
        inst.fc.resize(inst.numDC);
        inst.a_dist.resize(inst.numDC);
        inst.b.resize(inst.numDC);
        inst.beta.resize(inst.numDC);
        inst.w.resize(inst.numDC, 0.0);
        inst.retX.resize(inst.numRetailer);
        inst.retY.resize(inst.numRetailer);
        inst.mu.resize(inst.numRetailer);
        inst.variance.resize(inst.numRetailer);
        inst.reservePrice.resize(inst.numRetailer);
        inst.dist.resize(inst.numRetailer, std::vector<double>(inst.numDC));

        for (int j = 0; j < inst.numDC && std::getline(file, line); j++) {
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ','); inst.dcX[j] = std::stod(token);
            std::getline(ss, token, ','); inst.dcY[j] = std::stod(token);
            std::getline(ss, token, ','); inst.f[j] = std::stod(token);
            std::getline(ss, token, ','); inst.F[j] = std::stod(token);
            std::getline(ss, token, ','); inst.g[j] = std::stod(token);
            std::getline(ss, token, ','); inst.L[j] = std::stod(token);
            std::getline(ss, token, ','); inst.fc[j] = std::stod(token);
            std::getline(ss, token, ','); inst.a_dist[j] = std::stod(token);
            std::getline(ss, token, ','); inst.b[j] = std::stod(token);
            std::getline(ss, token, ','); inst.beta[j] = std::stod(token);
        }

        for (int i = 0; i < inst.numRetailer && std::getline(file, line); i++) {
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ','); inst.retX[i] = std::stod(token);
            std::getline(ss, token, ','); inst.retY[i] = std::stod(token);
            std::getline(ss, token, ','); inst.mu[i] = std::stoi(token);
            std::getline(ss, token, ','); inst.variance[i] = std::stod(token);
            std::getline(ss, token, ','); inst.reservePrice[i] = std::stod(token);
        }

        for (int i = 0; i < inst.numRetailer && std::getline(file, line); i++) {
            std::stringstream ss(line);
            for (int j = 0; j < inst.numDC; j++) {
                std::string token;
                std::getline(ss, token, ',');
                inst.dist[i][j] = std::stod(token);
            }
        }

        inst.dcToSupplierDist.resize(inst.numDC);
        for (int j = 0; j < inst.numDC; j++) {
            double dx = inst.dcX[j] - inst.supplierX;
            double dy = inst.dcY[j] - inst.supplierY;
            inst.dcToSupplierDist[j] = std::sqrt(dx * dx + dy * dy);
        }

        file.close();
        return inst;
    }

    static void saveToCSV(const Instance& inst, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[DataLoader] 无法写入 " << filename << std::endl;
            return;
        }

        file << inst.numDC << "," << inst.numRetailer << std::endl;

        for (int j = 0; j < inst.numDC; j++) {
            file << inst.dcX[j] << "," << inst.dcY[j] << ","
                 << inst.f[j] << "," << inst.F[j] << "," << inst.g[j] << ","
                 << inst.L[j] << "," << inst.fc[j] << "," << inst.a_dist[j] << ","
                 << inst.b[j] << "," << inst.beta[j] << std::endl;
        }

        for (int i = 0; i < inst.numRetailer; i++) {
            file << inst.retX[i] << "," << inst.retY[i] << ","
                 << inst.mu[i] << "," << inst.variance[i] << ","
                 << inst.reservePrice[i] << std::endl;
        }

        for (int i = 0; i < inst.numRetailer; i++) {
            for (int j = 0; j < inst.numDC; j++) {
                if (j > 0) file << ",";
                file << inst.dist[i][j];
            }
            file << std::endl;
        }

        file.close();
    }

private:
    DataLoader() = default;
};

#endif
