#ifndef TIMEMAN_H
#define TIMEMAN_H

#include "bitboard.h"
#include <cstdint>

class TimeManager {
public:
    TimeManager() = default;

    // Initialize time limits based on UCI go arguments
    void init(Color sideToMove, int movetime, int wtime, int btime, int winc, int binc);

    // Single-limit constructor for fixed movetime or tests
    void initFixedTime(int fixedTimeMs);

    int getOptimumTime() const { return optimumTime; }
    int getMaximumTime() const { return maximumTime; }
    bool isClockGame() const { return isClockGame_; }

    // Evaluates dynamic scaling (node fraction, instability) and decides if
    // search should stop before launching depth+1.
    bool shouldStopBeforeNextIteration(int depth,
                                       int64_t elapsedMs,
                                       double bestMoveNodeFraction,
                                       bool isUnstable) const;

private:
    int optimumTime = -1;
    int maximumTime = -1;
    bool isClockGame_ = false;
};

#endif // TIMEMAN_H
