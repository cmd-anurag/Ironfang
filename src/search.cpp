// search.cpp
#include "search.h"
#include "evaluate.h"
#include "zobrist.h"
#include "tt.h"
#include <cstdint>
#include <algorithm>
#include <array>
#include <chrono>
#include <assert.h>
#include <cstdlib>

std::vector<std::vector<Move>> killerMoves(MAX_PLY, std::vector<Move>(2, Move(NONE, -1, -1)));
std::array<std::array<std::array<int, 64>, 64>, 2> historyHeuristics{};
TranspositionTable TT;

uint64_t nodeCount = 0;
uint64_t quiescenceNodeCount = 0;
uint64_t betaCutoffCount = 0;
uint64_t firstMoveCutoffCount = 0;
int selectiveDepth = 0;

namespace {
constexpr uint64_t TIME_CHECK_NODE_INTERVAL = 1024;
constexpr uint64_t HALF_MOVE_TT_SALT = 0x9e3779b97f4a7c15ULL;
constexpr int HASH_MOVE_SCORE = 100000000;
constexpr int WINNING_CAPTURE_BIAS = 8000000;
constexpr int PROMOTION_BIAS = 6000000;
constexpr int KILLER_MOVE_BIAS = 4000000;
constexpr int LOSING_CAPTURE_BIAS = 2000000;
std::array<std::array<Move, MAX_PLY>, MAX_PLY> pvTable{};
std::array<int, MAX_PLY> pvLength{};
std::array<std::vector<Move>, MAX_PLY> searchMoveBuffers;
std::chrono::steady_clock::time_point hardSearchDeadline;
bool hasHardSearchDeadline = false;
bool searchStoppedByTime = false;
bool rootDiagnosticsEnabled = false;
uint64_t lateMoveReductionCount = 0;
uint64_t lateMoveResearchCount = 0;
uint64_t repetitionPathEntriesScanned = 0;

int squareForPieceSquareTable(Piece piece, int square) {
    return (piece & 7) == 0 || piece == NONE
        ? square
        : (getPieceColor(piece) == BLACK ? Evaluation::mirror(square) : square);
}

bool matchesHashMove(const Move& move, const Move& hashMove) {
    return hashMove.from >= 0 && move.from == hashMove.from &&
        move.to == hashMove.to && (move.promotion & 7) == (hashMove.promotion & 7);
}

int countCurrentRepetitionOccurrences(const BitBoard& board) {
    // A pawn move or capture makes every earlier position unreachable, and
    // Zobrist identity includes side to move. Only inspect same-side positions
    // within the reversible-move window instead of rescanning the full game.
    const int currentIndex = board.pathDepth - 1;
    const int earliestRelevantIndex = std::max(0, currentIndex - board.halfmoveClock);
    int occurrences = 0;
    for (int index = currentIndex; index >= earliestRelevantIndex; index -= 2) {
        ++repetitionPathEntriesScanned;
        if (board.repetitionPath[index] == board.zobristKey) {
            ++occurrences;
            if (occurrences >= 3) break;
        }
    }
    return occurrences;
}

struct AttackMaps {
    uint64_t all = 0;
    uint64_t pawns = 0;
};

AttackMaps attacksForSide(const BitBoard& board, Color side) {
    AttackMaps maps;
    const uint64_t occupied = board.getAllPieces();
    uint64_t pieces = side == WHITE ? board.getWhitePawns() : board.getBlackPawns();
    while (pieces) {
        const int square = board.popLSB(pieces);
        maps.pawns |= board.getPawnAttacks(square, side);
    }
    maps.all |= maps.pawns;

    pieces = side == WHITE ? board.getWhiteKnights() : board.getBlackKnights();
    while (pieces) maps.all |= board.getKnightAttacks(board.popLSB(pieces));
    pieces = side == WHITE ? board.getWhiteBishops() : board.getBlackBishops();
    while (pieces) maps.all |= board.getBishopAttacks(board.popLSB(pieces), occupied);
    pieces = side == WHITE ? board.getWhiteRooks() : board.getBlackRooks();
    while (pieces) maps.all |= board.getRookAttacks(board.popLSB(pieces), occupied);
    pieces = side == WHITE ? board.getWhiteQueens() : board.getBlackQueens();
    while (pieces) maps.all |= board.getQueenAttacks(board.popLSB(pieces), occupied);
    const int kingSquare = side == WHITE ? board.whiteKingSquare : board.blackKingSquare;
    maps.all |= board.getKingAttacks(kingSquare);
    return maps;
}

void orderMoves(BitBoard& board, std::vector<Move>& moves, const Move& hashMove,
                bool inQuiescence, int ply) {
    const Color mover = board.sideToMove;
    const Color opponent = mover == WHITE ? BLACK : WHITE;
    const size_t sideIndex = static_cast<size_t>(mover);
    const AttackMaps opponentAttacks = attacksForSide(board, opponent);

    for (Move& move : moves) {
        int score = 0;
        const bool isCapture = move.capture != NONE || move.isEnPassant;

        if (matchesHashMove(move, hashMove)) {
            move.heuristicScore = HASH_MOVE_SCORE;
            continue;
        }

        const int movingType = move.piece & 7;
        const int movingValue = Evaluation::pieceValue[movingType];
        if (isCapture) {
            const int capturedType = move.capture & 7;
            const int captureDelta = Evaluation::pieceValue[capturedType] - movingValue;
            const uint64_t targetMask = 1ULL << move.to;
            const bool attacked = (opponentAttacks.all & targetMask) != 0;
            score += attacked
                ? (captureDelta >= 0 ? WINNING_CAPTURE_BIAS : LOSING_CAPTURE_BIAS) + captureDelta
                : WINNING_CAPTURE_BIAS + captureDelta;
        }

        if (movingType == 1) {
            if (!isCapture && move.promotion && (move.promotion & 7) == 5)
                score += PROMOTION_BIAS;
        } else if (movingType != 6) {
            const int tableIndex = movingType - 1;
            if (tableIndex >= 0 && tableIndex < 5) {
                const int fromSquare = squareForPieceSquareTable(move.piece, move.from);
                const int toSquare = squareForPieceSquareTable(move.piece, move.to);
                score += PST[tableIndex][toSquare] - PST[tableIndex][fromSquare];
            }
        }

        if (movingType != 1 && movingType != 6) {
            const uint64_t targetMask = 1ULL << move.to;
            const bool pawnAttacked = (opponentAttacks.pawns & targetMask) != 0;
            if (pawnAttacked) score -= 50;
            else if (opponentAttacks.all & targetMask) score -= 25;
        }

        if (!isCapture) {
            if (!inQuiescence && ply >= 0 && ply < static_cast<int>(killerMoves.size())) {
                if (move == killerMoves[ply][0]) score += KILLER_MOVE_BIAS;
                else if (move == killerMoves[ply][1]) score += KILLER_MOVE_BIAS;
            }
            if (move.from >= 0 && move.from < 64 && move.to >= 0 && move.to < 64)
                score += historyHeuristics[sideIndex][move.from][move.to];
        }

        move.heuristicScore = score;
    }

    std::sort(moves.begin(), moves.end(), [](const Move& lhs, const Move& rhs) {
        return lhs.heuristicScore > rhs.heuristicScore;
    });
}

void updatePrincipalVariation(int ply, const Move& move) {
    if (ply < 0 || ply >= MAX_PLY - 1) {
        return;
    }

    pvTable[ply][ply] = move;
    const int childLength = std::min(pvLength[ply + 1], MAX_PLY);
    for (int index = ply + 1; index < childLength; ++index) {
        pvTable[ply][index] = pvTable[ply + 1][index];
    }
    pvLength[ply] = std::max(ply + 1, childLength);
}

bool checkSearchDeadline(bool force = false) {
    if (searchStoppedByTime) {
        return true;
    }
    if (!hasHardSearchDeadline) {
        return false;
    }
    if (!force && (nodeCount % TIME_CHECK_NODE_INTERVAL) != 0) {
        return false;
    }

    if (std::chrono::steady_clock::now() >= hardSearchDeadline) {
        searchStoppedByTime = true;
    }
    return searchStoppedByTime;
}

uint64_t transpositionKey(const BitBoard& board) {
    // Keep repetition identity based on board.zobristKey, while separating TT
    // scores whose validity depends on the fifty-move counter.
    return board.zobristKey ^ (static_cast<uint64_t>(board.halfmoveClock) * HALF_MOVE_TT_SALT);
}

bool hasFiftyMoveClaimByIntendedQuietMove(BitBoard& board) {
    if (board.halfmoveClock != 99) {
        return false;
    }

    std::vector<Move> moves = board.generateMoves();
    for (const Move& move : moves) {
        if (move.piece == WP || move.piece == BP || move.capture || move.isEnPassant) {
            continue;
        }

        Gamestate previousState = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare,
            board.zobristKey,
            board.halfmoveClock
        };

        if (board.makeMove(move)) {
            board.unmakeMove(move, previousState);
            return true;
        }
    }

    return false;
}

// The fifty-move rule is claimable after 100 halfmoves without a pawn move or
// capture. Checkmate takes precedence when the counter is already at the limit.
bool adjudicateFiftyMoveRule(BitBoard& board, int ply, int& score) {
    if (board.halfmoveClock < 100) {
        return false;
    }

    int kingSquare = board.sideToMove == WHITE ? board.whiteKingSquare : board.blackKingSquare;
    Color opponent = board.sideToMove == WHITE ? BLACK : WHITE;
    if (!board.isSquareAttacked(kingSquare, opponent)) {
        score = 0;
        return true;
    }

    std::vector<Move> moves = board.generateMoves();
    for (const Move& move : moves) {
        Gamestate previousState = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare,
            board.zobristKey,
            board.halfmoveClock
        };

        if (board.makeMove(move)) {
            board.unmakeMove(move, previousState);
            score = 0;
            return true;
        }
    }

    score = -MATE_SCORE + ply;
    return true;
}
}

void Search::setRootDiagnostics(bool enabled) {
    rootDiagnosticsEnabled = enabled;
}

static int scoreToTT(int score, int ply) {
    if (score >= MATE_THRESHOLD) return score + ply;
    if (score <= -MATE_THRESHOLD) return score - ply;
    return score;
}

Move Search::findBestMove(BitBoard& board, int maxDepth, int timeLimit) {
    maxDepth = std::max(1, std::min(maxDepth, MAX_DEPTH));
    // Reset counters
    nodeCount = 0;
    quiescenceNodeCount = 0;
    betaCutoffCount = 0;
    firstMoveCutoffCount = 0;
    selectiveDepth = 0;
    lateMoveReductionCount = 0;
    lateMoveResearchCount = 0;
    repetitionPathEntriesScanned = 0;
    pvLength.fill(0);
    historyHeuristics = {};
    TT.lookupCount = 0;
    TT.hitCount = 0;
    TT.keyMatchCount = 0;
    TT.totalStoreAttempts = 0;
    TT.actualStores = 0;
    TT.overwritten = 0;
    
    // Start timing
    auto startTime = std::chrono::steady_clock::now();
    searchStoppedByTime = false;

    auto emitSearchStats = [&]() {
        const auto statsElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        std::cout << "info string stats"
                  << " total_nodes=" << nodeCount
                  << " elapsed_ms=" << statsElapsedMs
                  << " qnodes=" << quiescenceNodeCount
                  << " tt_probes=" << TT.lookupCount
                  << " tt_keyhits=" << TT.keyMatchCount
                  << " tt_cutoffs=" << TT.hitCount
                  << " tt_store_attempts=" << TT.totalStoreAttempts
                  << " tt_writes=" << TT.actualStores
                  << " tt_overwrites=" << TT.overwritten
                  << " ab_cutoffs=" << betaCutoffCount
                  << " first_move_cutoffs=" << firstMoveCutoffCount
                  << " lmr_reductions=" << lateMoveReductionCount
                  << " lmr_researches=" << lateMoveResearchCount
                  << " repetition_scan_entries=" << repetitionPathEntriesScanned;
        if (searchStoppedByTime) {
            std::cout << " stop=hard_time";
        }
        std::cout << "\n" << std::flush;
    };
    
    // Set a hard cutoff time slightly below the time limit (95%)
    int hardTimeLimit = timeLimit > 0 ? std::max(1, static_cast<int>(timeLimit * 0.95)) : -1;
    hasHardSearchDeadline = hardTimeLimit > 0;
    if (hasHardSearchDeadline) {
        hardSearchDeadline = startTime + std::chrono::milliseconds(hardTimeLimit);
    }
    
    // Default move (will be overwritten soon)
    Move bestMove{NONE, -1, -1};
    int bestScore = -INF;
    
    // Generate all  moves
    std::vector<Move> moves = board.generateMoves();
    
    if (moves.empty()) {
        return Move{NONE, -1, -1}; // No moves available
    }
    // Keep the game history supplied by the UCI layer. Direct callers may not
    // have seeded it, so ensure the current root is present exactly once.
    if (board.pathDepth == 0 ||
        board.repetitionPath[board.pathDepth - 1] != board.zobristKey) {
        if (board.pathDepth >= 1024 - MAX_PLY - 1) {
            for (int i = 1; i < board.pathDepth; ++i)
                board.repetitionPath[i - 1] = board.repetitionPath[i];
            --board.pathDepth;
        }
        board.repetitionPath[board.pathDepth++] = board.zobristKey;
    }
    
    // Set a default move immediately for safety
    bool hasLegalRootMove = false;
    for (const Move& move : moves) {
        Gamestate prevdata = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare,
            board.zobristKey,
            board.halfmoveClock
        };
        
        if (board.makeMove(move)) {
            board.unmakeMove(move, prevdata);
            bestMove = move; // Set default move in case we run out of time
            hasLegalRootMove = true;
            break;
        }
    }
    if (!hasLegalRootMove) return Move{NONE, -1, -1};

    std::array<Move, MAX_PLY> bestPV{};
    int bestPVLength = 1;
    bestPV[0] = bestMove;
    
    // ------------------------------------------------------------------------------------
    //                          ITERATIVE DEEPENING SEARCH
    // ------------------------------------------------------------------------------------
    for (int depth = 1; depth <= maxDepth; depth++) {
        selectiveDepth = 0;

        if (depth > 1 && checkSearchDeadline(true)) {
            break;
        }

        // Start a new iteration
        int alpha = -INF;
        int beta = INF;
        int iterationBestScore = -INF;
        Move iterationBestMove = bestMove; // Start with previous best
        std::array<Move, MAX_PLY> iterationPV{};
        int iterationPVLength = 0;
        
        Move rootHashMove(NONE, -1, -1);
        int ignoredRootEval = 0;
        TT.probe(transpositionKey(board), depth, -INF, INF, 0,
                 ignoredRootEval, rootHashMove, false);
        if (rootHashMove.from < 0) rootHashMove = bestMove;
        orderMoves(board, moves, rootHashMove, false, 0);

        // The generator includes pseudo-legal moves; reductions should be
        // counted over successfully made (legal) moves, like the reference's
        // legal move list.
        size_t legalRootMoveCount = 0;
        // Match the reference's full alpha-beta root search. The root window
        // narrows as alpha improves; it does not use a separate PVS probe.
        for (size_t rootMoveIndex = 0; rootMoveIndex < moves.size(); ++rootMoveIndex) {
            const Move& move = moves[rootMoveIndex];
            Gamestate prevdata = {
                board.sideToMove,
                board.enPassantSquare,
                board.whiteKingsideCastle,
                board.whiteQueensideCastle,
                board.blackKingsideCastle,
                board.blackQueensideCastle,
                board.whiteKingSquare,
                board.blackKingSquare,
                board.zobristKey,
                board.halfmoveClock
            };
            
            if (!board.makeMove(move)) {
                continue;
            }
            const size_t legalRootMoveIndex = legalRootMoveCount++;

            int rootExtension = 0;
            if (MAX_EXTENSIONS > 0) {
                const int defendingKing = board.sideToMove == WHITE
                    ? board.whiteKingSquare
                    : board.blackKingSquare;
                const bool givesCheck = board.isSquareAttacked(defendingKing, prevdata.sideToMove);
                const int targetRank = move.to / 8;
                const bool advancedPawn = (move.piece & 7) == 1 &&
                    (targetRank == 1 || targetRank == 6);
                if (givesCheck || advancedPawn) rootExtension = 1;
            }

            const uint64_t nodesBefore = nodeCount;
            const uint64_t reductionsBefore = lateMoveReductionCount;
            const uint64_t researchesBefore = lateMoveResearchCount;
            int score;
            const bool isCapture = move.capture != NONE || move.isEnPassant;
            const bool reduceRootMove = !rootDiagnosticsEnabled && depth >= 3 &&
                legalRootMoveIndex >= 3 && rootExtension == 0 && !isCapture;
            if (reduceRootMove) {
                ++lateMoveReductionCount;
                score = -minimaxAlphaBeta(board, depth - 2, -alpha - 1, -alpha,
                                          1, rootExtension);
                if (!searchStoppedByTime && score > alpha) {
                    ++lateMoveResearchCount;
                    score = -minimaxAlphaBeta(board, depth - 1 + rootExtension,
                                              -beta, -alpha, 1, rootExtension);
                }
            } else if (rootDiagnosticsEnabled) {
                score = -minimaxAlphaBeta(board, depth - 1 + rootExtension,
                                          -INF, INF, 1, rootExtension);
            } else {
                score = -minimaxAlphaBeta(board, depth - 1 + rootExtension,
                                          -beta, -alpha, 1, rootExtension);
            }

            std::array<Move, MAX_PLY> candidatePV{};
            int candidatePVLength = 1;
            candidatePV[0] = move;
            if (rootDiagnosticsEnabled) {
                const int continuationLength = std::max(0, std::min(pvLength[1] - 1, MAX_PLY - 2));
                for (int index = 0; index < continuationLength; ++index) {
                    candidatePV[index + 1] = pvTable[1][index + 1];
                }
                candidatePVLength += continuationLength;
            }
            
            board.unmakeMove(move, prevdata);

            // A timed-out recursive search returns a neutral placeholder score.
            // Discard the incomplete iteration and keep the last completed move.
            if (checkSearchDeadline(true)) {
                emitSearchStats();
                return bestMove;
            }

            if (rootDiagnosticsEnabled) {
                std::cout << "info string root depth=" << depth
                          << " move=" << moveToUCI(move)
                          << " score=" << score
                          << " nodes=" << (nodeCount - nodesBefore)
                          << " lmr=" << (lateMoveReductionCount - reductionsBefore)
                          << " lmr_researches=" << (lateMoveResearchCount - researchesBefore)
                          << " pv";
                for (int index = 0; index < candidatePVLength; ++index) {
                    std::cout << ' ' << moveToUCI(candidatePV[index]);
                }
                std::cout << "\n" << std::flush;
            }
            
            // Check if this is a better move
            if (score > iterationBestScore) {
                iterationBestScore = score;
                iterationBestMove = move;
                iterationPV[0] = move;
                const int continuationLength = std::max(0, std::min(pvLength[1] - 1, MAX_PLY - 2));
                for (int index = 0; index < continuationLength; ++index) {
                    iterationPV[index + 1] = pvTable[1][index + 1];
                }
                iterationPVLength = continuationLength + 1;
                
                if (score > alpha) {
                    alpha = score;
                }
            }
            
        }
        
        // Only update bestMove if we completed the iteration
        bestMove = iterationBestMove;
        bestScore = iterationBestScore;
        bestPV = iterationPV;
        bestPVLength = iterationPVLength;
        TT.store(transpositionKey(board), depth, scoreToTT(bestScore, 0), TT_EXACT, bestMove);

        int hashFull = TT.hashfull();

        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - startTime).count();
        
        const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
            currentTime - startTime).count();
        const uint64_t nps = elapsedUs > 0
            ? (nodeCount * 1000000) / static_cast<uint64_t>(elapsedUs)
            : 0;

        // Output information
        std::cout << "info depth " << depth
                  << " seldepth " << selectiveDepth
                  << " time " << elapsedMs
                  << " score ";
        if (bestScore >= MATE_THRESHOLD || bestScore <= -MATE_THRESHOLD) {
            int pliesToMate = MATE_SCORE - std::abs(bestScore);
            int mateMoves = (pliesToMate + 1) / 2;
            std::cout << "mate " << (bestScore > 0 ? mateMoves : -mateMoves);
        } else {
            std::cout << "cp " << bestScore;
        }
        std::cout << " nodes " << nodeCount << " hashfull " << hashFull
                  << " nps " << nps << " pv";
        for (int index = 0; index < bestPVLength; ++index) {
            std::cout << ' ' << moveToUCI(bestPV[index]);
        }
        std::cout << "\n" << std::flush;
    }

    emitSearchStats();
    return bestMove;
}

int Search::minimaxAlphaBeta(BitBoard& board, int depth, int alpha, int beta, int ply,
                             int numExtensions) {
    ++nodeCount; // an efficiency metric
    selectiveDepth = std::max(selectiveDepth, ply);
    if (ply < MAX_PLY) {
        pvLength[ply] = ply;
    }

    if (checkSearchDeadline()) {
        return 0;
    }

    if (ply >= MAX_PLY - 1) {
        return Evaluation::evaluate(board);
    }

    // Keep mate scores distance-sensitive, as in the reference search.
    alpha = std::max(alpha, -MATE_SCORE + ply);
    beta = std::min(beta, MATE_SCORE - ply);
    if (alpha >= beta) return alpha;

    board.repetitionPath[board.pathDepth++] = board.zobristKey;
    assert(board.pathDepth <= 1024);

    const int repetitionOccurrences = countCurrentRepetitionOccurrences(board);
    if (repetitionOccurrences >= 3) {
        --board.pathDepth;
        return 0;
    }

    const int originalAlpha = alpha;
    int ruleScore;
    if (adjudicateFiftyMoveRule(board, ply, ruleScore)) {
        --board.pathDepth;
        return ruleScore;
    }
    if (hasFiftyMoveClaimByIntendedQuietMove(board)) {
        alpha = std::max(alpha, 0);
        if (alpha >= beta) {
            --board.pathDepth;
            return alpha;
        }
    }

    // Probe the transposition table before generating moves. A cached score
    // may be unsafe when this position is already present in the repetition path,
    // but its move is still useful for ordering.
    Move tempMove = Move(NONE, -1, -1);
    int tempEval;

    // A cached score can depend on a different repetition history. Keep the
    // TT move for ordering, but search this node when it has occurred before.
    if (TT.probe(transpositionKey(board), depth, alpha, beta, ply, tempEval, tempMove,
                 repetitionOccurrences == 1)) {
        --board.pathDepth;
        return tempEval;
    }

    if (depth <= 0) {
        --board.pathDepth;
        return quiescenceSearch(board, alpha, beta, 0, ply);
    }

    std::vector<Move>& moves = searchMoveBuffers[ply];
    board.generateMoves(moves);
    if (moves.empty()) {
        // No pseudo‐legal moves at all → either checkmate or stalemate
        int kingSq = (board.sideToMove == WHITE)
                       ? board.whiteKingSquare
                       : board.blackKingSquare;
        Color opp = (board.sideToMove == WHITE) ? BLACK : WHITE;

        if (board.isSquareAttacked(kingSq, opp)) {
            // Checkmate: encode ply distance so mate in 1 is better than mate in 2
            --board.pathDepth;
            return -MATE_SCORE + ply;
        }
        --board.pathDepth;
        return 0; // stalemate
    }

    orderMoves(board, moves, tempMove, false, ply);

    bool foundLegalMove = false;
    Move bestMove(NONE, -1, -1);
    int legalMoveIndex = 0;
    for (size_t moveIndex = 0; moveIndex < moves.size(); ++moveIndex) {
        const Move& move = moves[moveIndex];
        Gamestate prevdata = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare,
            board.zobristKey,
            board.halfmoveClock
        };

        // uint64_t originalKey = board.zobristKey;
        if (!board.makeMove(move)) {
            continue;
        }
        foundLegalMove = true;

        int extension = 0;
        if (numExtensions < MAX_EXTENSIONS) {
            const int defendingKing = board.sideToMove == WHITE
                ? board.whiteKingSquare
                : board.blackKingSquare;
            const bool givesCheck = board.isSquareAttacked(defendingKing, prevdata.sideToMove);
            const int targetRank = move.to / 8;
            const bool advancedPawn = (move.piece & 7) == 1 &&
                (targetRank == 1 || targetRank == 6);
            if (givesCheck || advancedPawn) extension = 1;
        }

        const bool isCapture = move.capture != NONE || move.isEnPassant;
        // legalMoveIndex excludes pseudo-legal moves rejected by makeMove.
        const bool reduceMove = extension == 0 && depth >= 3 && legalMoveIndex >= 3 &&
            !isCapture;

        int score;
        if (reduceMove) {
            ++lateMoveReductionCount;
            score = -minimaxAlphaBeta(board, depth - 2, -alpha - 1, -alpha,
                                      ply + 1, numExtensions);
            if (!searchStoppedByTime && score > alpha) {
                ++lateMoveResearchCount;
                score = -minimaxAlphaBeta(board, depth - 1 + extension, -beta, -alpha,
                                          ply + 1, numExtensions + extension);
            }
        } else {
            score = -minimaxAlphaBeta(board, depth - 1 + extension, -beta, -alpha,
                                      ply + 1, numExtensions + extension);
        }
        board.unmakeMove(move, prevdata);
        ++legalMoveIndex;

        if (searchStoppedByTime) {
            --board.pathDepth;
            return 0;
        }

        // Beta cutoff
        if (score >= beta) {
            ++betaCutoffCount;
            if (legalMoveIndex == 1) ++firstMoveCutoffCount;
            updatePrincipalVariation(ply, move);

            if (!isCapture && ply < static_cast<int>(killerMoves.size()) &&
                !(move == killerMoves[ply][0])) {
                killerMoves[ply][1] = killerMoves[ply][0];
                killerMoves[ply][0] = move;
            }
            const size_t sideIndex = static_cast<size_t>(prevdata.sideToMove);
            if (!isCapture && move.from >= 0 && move.from < 64 && move.to >= 0 && move.to < 64) {
                int& history = historyHeuristics[sideIndex][move.from][move.to];
                history = std::min(100000, history + depth * depth);
            }
            TT.store(transpositionKey(board), depth, scoreToTT(beta, ply), TT_LOWER, move);

            --board.pathDepth;
            return beta;
        }

        if (score > alpha) {
            alpha = score;
            bestMove = move;
            updatePrincipalVariation(ply, move);
        }
    }

    //  there were pseudo‐legal moves but none was actually legal
    if (!foundLegalMove) {
        int kingSq = (board.sideToMove == WHITE)
                       ? board.whiteKingSquare
                       : board.blackKingSquare;
        Color opp = (board.sideToMove == WHITE) ? BLACK : WHITE;

        int eval = board.isSquareAttacked(kingSq, opp)
        ? -MATE_SCORE + ply  // checkmate
        : 0;             // stalemate

        TT.store(transpositionKey(board), depth, scoreToTT(eval, ply), TT_EXACT, Move(NONE, -1, -1));
        --board.pathDepth;
        return eval;
    }

    TT_FLAG flag;
    if(alpha <= originalAlpha) {
        flag = TT_UPPER;
    }
    else {
        flag = TT_EXACT;
    }

    TT.store(transpositionKey(board), depth, scoreToTT(alpha, ply), flag, bestMove);
    --board.pathDepth;
    return alpha;
}

int Search::quiescenceSearch(BitBoard& board, int alpha, int beta, int qdepth, int ply) {
    ++nodeCount;
    ++quiescenceNodeCount;
    selectiveDepth = std::max(selectiveDepth, ply);
    if (ply < MAX_PLY) pvLength[ply] = ply;

    if (checkSearchDeadline()) return 0;
    if (ply >= MAX_PLY - 1) return Evaluation::evaluate(board);
    if (board.pathDepth >= 1024) return Evaluation::evaluate(board);

    board.repetitionPath[board.pathDepth++] = board.zobristKey;
    const int repetitionOccurrences = countCurrentRepetitionOccurrences(board);
    if (repetitionOccurrences >= 3) {
        --board.pathDepth;
        return 0;
    }

    int ruleScore;
    if (adjudicateFiftyMoveRule(board, ply, ruleScore)) {
        --board.pathDepth;
        return ruleScore;
    }
    if (hasFiftyMoveClaimByIntendedQuietMove(board)) {
        alpha = std::max(alpha, 0);
        if (alpha >= beta) {
            --board.pathDepth;
            return alpha;
        }
    }

    const int kingSquare = board.sideToMove == WHITE
        ? board.whiteKingSquare
        : board.blackKingSquare;
    const Color opponent = board.sideToMove == WHITE ? BLACK : WHITE;
    const bool inCheck = board.isSquareAttacked(kingSquare, opponent);

    if (!inCheck) {
        const int standPat = Evaluation::evaluate(board);
        if (standPat >= beta) {
            --board.pathDepth;
            return beta;
        }
        alpha = std::max(alpha, standPat);
    }

    std::vector<Move>& moves = searchMoveBuffers[ply];
    if (inCheck) board.generateMoves(moves);
    else board.generateCaptures(moves);
    orderMoves(board, moves, Move(NONE, -1, -1), true, ply);

    bool foundLegalMove = false;
    for (const Move& move : moves) {
        Gamestate previousState = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare,
            board.zobristKey,
            board.halfmoveClock
        };

        if (!board.makeMove(move)) continue;
        foundLegalMove = true;
        const int score = -quiescenceSearch(board, -beta, -alpha, qdepth + 1, ply + 1);
        board.unmakeMove(move, previousState);

        if (searchStoppedByTime) {
            --board.pathDepth;
            return 0;
        }
        if (score >= beta) {
            updatePrincipalVariation(ply, move);
            --board.pathDepth;
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            updatePrincipalVariation(ply, move);
        }
    }

    if (inCheck && !foundLegalMove) {
        --board.pathDepth;
        return -MATE_SCORE + ply;
    }

    --board.pathDepth;
    return alpha;
}
