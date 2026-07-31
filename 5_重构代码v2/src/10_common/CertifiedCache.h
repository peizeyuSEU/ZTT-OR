#ifndef CERTIFIED_CACHE_H
#define CERTIFIED_CACHE_H

#include "Types.h"

namespace CertifiedCache {

inline bool isCacheable(SolveStatus status, bool hasIntegerSolution) {
    return status == SolveStatus::OPTIMAL && hasIntegerSolution;
}

template <typename Map, typename Key, typename Value>
bool store(Map& cache, const Key& key, const Value& value,
           SolveStatus status, bool hasIntegerSolution) {
    if (!isCacheable(status, hasIntegerSolution)) return false;
    cache[key] = value;
    return true;
}

}  // namespace CertifiedCache

#endif  // CERTIFIED_CACHE_H
