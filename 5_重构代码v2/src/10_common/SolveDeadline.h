#ifndef SOLVE_DEADLINE_H
#define SOLVE_DEADLINE_H

#include <algorithm>
#include <chrono>
#include <limits>

class SolveDeadline {
public:
    using Clock = std::chrono::steady_clock;

    explicit SolveDeadline(double limitSeconds)
        : enabled_(limitSeconds > 0.0),
          deadline_(enabled_
                        ? Clock::now()
                              + std::chrono::duration_cast<Clock::duration>(
                                    std::chrono::duration<double>(limitSeconds))
                        : Clock::time_point::max()) {}

    bool enabled() const { return enabled_; }

    bool expired() const {
        return enabled_ && Clock::now() >= deadline_;
    }

    double remainingSeconds() const {
        if (!enabled_) return std::numeric_limits<double>::infinity();
        return std::max(
            0.0,
            std::chrono::duration<double>(deadline_ - Clock::now()).count());
    }

private:
    bool enabled_;
    Clock::time_point deadline_;
};

#endif  // SOLVE_DEADLINE_H
