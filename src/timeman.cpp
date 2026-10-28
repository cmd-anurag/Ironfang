#include "timeman.h"
#include <algorithm>
#include <cmath>

void TimeManager::initFixedTime(int fixedTimeMs) {
    optimumTime = fixedTimeMs;
    maximumTime = fixedTimeMs;
    isClockGame_ = false;
}

void TimeManager::init(Color sideToMove, int movetime, int wtime, int btime, int winc, int binc) {
    if (movetime >= 0) {
        initFixedTime(movetime);
        return;
    }

    const bool hasClock = (sideToMove == WHITE && wtime >= 0) ||
                          (sideToMove == BLACK && btime >= 0);
    if (!hasClock) {
        optimumTime = -1;
        maximumTime = -1;
        isClockGame_ = false;
        return;
    }

    isClockGame_ = true;
    const int timeLeft = (sideToMove == WHITE) ? wtime : btime;
    const int increment = (sideToMove == WHITE) ? winc : binc;

    // Move overhead / safety buffer to prevent time forfeit on pipe / GUI latency.
    const int safetyBuffer = increment > 0 ? 15 : 30;
    const int effectiveTimeLeft = std::max(1, timeLeft - safetyBuffer);

    // Stockfish-style dual bounds:
    // Sudden-death target: ~35 remaining moves + 75% of increment.
    double plannedTime = effectiveTimeLeft / 35.0;
    if (effectiveTimeLeft > increment * 2) {
        plannedTime += increment * 0.75;
    }
    const double minimumThinkTime = std::min(10.0, effectiveTimeLeft * 0.25);
    optimumTime = static_cast<int>(std::ceil(std::max(minimumThinkTime, plannedTime)));
    optimumTime = std::min(optimumTime, effectiveTimeLeft);

    // Hard emergency ceiling: at most 30% of remaining clock, or 3.5x optimumTime
    maximumTime = std::min(static_cast<int>(effectiveTimeLeft * 0.30),
                           static_cast<int>(optimumTime * 3.5));
    maximumTime = std::max(optimumTime, maximumTime);
    maximumTime = std::min(maximumTime, effectiveTimeLeft);
}

bool TimeManager::shouldStopBeforeNextIteration(int depth,
                                               int64_t elapsedMs,
                                               double bestMoveNodeFraction,
                                               bool isUnstable) const {
    if (!isClockGame_ || optimumTime <= 0) {
        return false;
    }

    double scaleFactor = 1.0;

    // 1. Obvious move / singularity saving:
    // If at depth >= 6 a single move soaked up >= 75% of root nodes,
    // the move is dominant. We scale down target time to bank time.
    if (depth >= 6 && bestMoveNodeFraction >= 0.75) {
        scaleFactor = std::max(0.5, 1.5 - bestMoveNodeFraction);
    }

    // 2. Instability & Crisis extension:
    // If the best move changed from the previous depth, or the score plummeted
    // by > 30 cp (fail low / crisis), extend time up to 1.8x to resolve the crisis.
    if (isUnstable) {
        scaleFactor = std::max(scaleFactor, 1.8);
    }

    const int targetTime = std::min(maximumTime, static_cast<int>(optimumTime * scaleFactor));

    // Stop before launching depth+1 if:
    // a) We reached the target time, OR
    // b) We have used more than 55% of target time on depth >= 5 and position is stable.
    if (elapsedMs >= targetTime ||
        (depth >= 5 && elapsedMs >= targetTime * 0.55 && !isUnstable)) {
        return true;
    }

    return false;
}
