#ifndef GA_FITNESS_UTILS_H
#define GA_FITNESS_UTILS_H

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace GAFitnessUtils {

inline bool isValid(double value) {
    return std::isfinite(value) && value > -DBL_MAX / 2.0;
}

inline std::vector<double> selectionWeights(
    const std::vector<double>& fitness) {
    std::vector<double> weights(fitness.size(), 0.0);

    bool foundValid = false;
    long double minValid = 0.0L;
    long double maxValid = 0.0L;
    for (double value : fitness) {
        if (!isValid(value)) continue;
        const long double extended = static_cast<long double>(value);
        if (!foundValid) {
            minValid = maxValid = extended;
            foundValid = true;
        } else {
            if (extended < minValid) minValid = extended;
            if (extended > maxValid) maxValid = extended;
        }
    }

    if (!foundValid) return weights;

    const long double range = maxValid - minValid;
    if (range == 0.0L) {
        for (size_t i = 0; i < fitness.size(); ++i) {
            if (isValid(fitness[i])) weights[i] = 1.0;
        }
        return weights;
    }

    for (size_t i = 0; i < fitness.size(); ++i) {
        if (!isValid(fitness[i])) continue;
        weights[i] = static_cast<double>(
            (static_cast<long double>(fitness[i]) - minValid) / range);
    }
    return weights;
}

inline std::vector<std::vector<double>> replaceWithElite(
    const std::vector<std::vector<double>>& population,
    const std::vector<double>& fitness,
    const std::vector<std::vector<double>>& offspring,
    bool elitism) {
    if (!elitism || population.empty() || offspring.empty()) {
        return offspring;
    }

    int bestIndex = -1;
    const size_t count = std::min(population.size(), fitness.size());
    for (size_t i = 0; i < count; ++i) {
        if (!isValid(fitness[i])) continue;
        if (bestIndex < 0 || fitness[i] > fitness[bestIndex]) {
            bestIndex = static_cast<int>(i);
        }
    }
    if (bestIndex < 0) return offspring;

    // 先完整保留 offspring，再用上一代精英覆盖固定位置。上一代精英的
    // 索引与 offspring 的索引没有语义关系，不能据此跳过某个后代。
    std::vector<std::vector<double>> nextPopulation = offspring;
    nextPopulation[0] = population[bestIndex];
    return nextPopulation;
}

}  // namespace GAFitnessUtils

#endif  // GA_FITNESS_UTILS_H
