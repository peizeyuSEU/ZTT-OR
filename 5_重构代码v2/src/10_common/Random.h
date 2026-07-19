#ifndef RANDOM_H
#define RANDOM_H

#include <random>
#include <cstdint>

/**
 * 固定种子的随机数生成器
 * 确保可复现性：同一种子运行两次，结果完全一致
 *
 * 用法：
 *   Random rng(42);
 *   double x = rng.uniform(0.0, 1.0);
 *   int n = rng.uniformInt(1, 100);
 *   double g = rng.normal(0.0, 1.0);
 */
class Random {
private:
    std::mt19937 rng;

public:
    /**
     * @param seed 随机种子，固定种子可复现结果
     */
    explicit Random(uint32_t seed) : rng(seed) {}

    /**
     * 生成 [min, max] 范围内的均匀分布浮点数
     */
    double uniform(double min = 0.0, double max = 1.0) {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng);
    }

    /**
     * 生成 [min, max] 范围内的均匀分布整数
     */
    int uniformInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    /**
     * 生成正态分布（高斯分布）随机数
     */
    double normal(double mean = 0.0, double stddev = 1.0) {
        std::normal_distribution<double> dist(mean, stddev);
        return dist(rng);
    }

    /**
     * 生成 0 或 1 的随机整数（伯努利试验）
     */
    int bernoulli(double p = 0.5) {
        std::bernoulli_distribution dist(p);
        return dist(rng) ? 1 : 0;
    }

    /**
     * 重新设置种子（用于重置随机序列）
     */
    void setSeed(uint32_t seed) {
        rng.seed(seed);
    }

    /** 获取底层引擎的引用 */
    std::mt19937& engine() { return rng; }
};

#endif // RANDOM_H
