#ifndef EXACT_W_KEY_H
#define EXACT_W_KEY_H

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace ExactWKey {

inline std::string make(const std::vector<double>& w) {
    static_assert(sizeof(double) == sizeof(std::uint64_t),
                  "Exact w cache keys require 64-bit IEEE doubles");
    static_assert(std::numeric_limits<double>::is_iec559,
                  "Exact w cache keys require IEEE-754 doubles");

    std::string key;
    key.reserve(w.size() * sizeof(std::uint64_t));
    for (double value : w) {
        // Treat +0 and -0 as the same mathematical reduction rate.
        if (value == 0.0) value = 0.0;
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        key.append(reinterpret_cast<const char*>(&bits), sizeof(bits));
    }
    return key;
}

}  // namespace ExactWKey

#endif  // EXACT_W_KEY_H
