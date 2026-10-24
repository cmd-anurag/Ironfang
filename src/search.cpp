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
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <type_traits>

std::array<std::array<uint16_t, 2>, MAX_PLY> killerMoves{};
std::array<std::array<std::array<int, 64>, 64>, 2> historyHeuristics{};
TranspositionTable TT;
std::mutex uciOutputMutex;

uint64_t nodeCount = 0;
uint64_t quiescenceNodeCount = 0;
uint64_t betaCutoffCount = 0;
uint64_t firstMoveCutoffCount = 0;
int selectiveDepth = 0;

namespace {
constexpr int HASH_MOVE_SCORE = 100000000;
constexpr int WINNING_CAPTURE_BIAS = 8000000;
constexpr int LOSING_CAPTURE_BIAS = 2000000;
constexpr int PROMOTION_BIAS = 6000000;
constexpr int KILLER_MOVE_BIAS = 4000000;
constexpr int MAX_KILLER_PLY = 32;
constexpr int MAX_SEARCH_MOVES = 218;

struct SearchMove {
    uint16_t value = 0;

    SearchMove() = default;
    explicit SearchMove(uint16_t encoded) : value(encoded) {}
};

struct SearchMoveOutput {
    using value_type = SearchMove;
    SearchMove* data;
    int count = 0;

    void clear() { count = 0; }
    void push_back(const SearchMove& move) { data[count++] = move; }
};

int referenceMoveFlag(const Move& move);

uint16_t packSearchMove(const Move& move) {
    if (move.from < 0 || move.to < 0) return 0;
    return static_cast<uint16_t>(
        move.from | (move.to << 6) | (referenceMoveFlag(move) << 12));
}

int searchMoveFrom(const SearchMove& move) {
    return move.value & 0x3f;
}

int searchMoveTo(const SearchMove& move) {
    return (move.value >> 6) & 0x3f;
}

int searchMoveFlag(const SearchMove& move) {
    return move.value >> 12;
}

Move decodeSearchMove(const BitBoard& board, const SearchMove& searchMove) {
    const int from = searchMoveFrom(searchMove);
    const int to = searchMoveTo(searchMove);
    const int flag = searchMoveFlag(searchMove);
    const Piece piece = board.getPiece(from);
    const bool enPassant = flag == 1;
    const bool kingSideCastle = flag == 2 && to > from;
    const bool queenSideCastle = flag == 2 && to < from;
    int promotion = 0;
    if (flag >= 4) {
        const bool white = getPieceColor(piece) == WHITE;
        promotion = flag == 4 ? (white ? WQ : BQ)
            : flag == 5 ? (white ? WN : BN)
            : flag == 6 ? (white ? WR : BR)
            : (white ? WB : BB);
    }
    Piece capture = board.getPiece(to);
    if (enPassant) capture = piece == WP ? BP : WP;
    return Move(piece, from, to, promotion, capture,
                kingSideCastle, queenSideCastle, enPassant);
}

std::array<std::array<Move, MAX_PLY>, MAX_PLY> pvTable{};
std::array<int, MAX_PLY> pvLength{};
std::array<std::array<SearchMove, MAX_SEARCH_MOVES>, MAX_PLY> searchMoveBuffers;
std::array<std::array<int, MAX_SEARCH_MOVES>, MAX_PLY> searchMoveScores;
std::array<int, MAX_PLY> searchMoveCounts{};
std::chrono::steady_clock::time_point hardSearchDeadline;
bool hasHardSearchDeadline = false;
std::atomic<bool> searchStoppedByTime{false};
std::atomic<bool> searchStopRequested{false};
bool rootDiagnosticsEnabled = false;
Move iterationBestMove(NONE, -1, -1);
int iterationBestScore = -INF;
bool hasSearchedAtLeastOneRootMove = false;
uint16_t previousRootBestMove = 0;
uint64_t lateMoveReductionCount = 0;
uint64_t lateMoveResearchCount = 0;
uint64_t pvsResearchCount = 0;
uint64_t nullMoveTryCount = 0;
uint64_t nullMoveCutoffCount = 0;
uint64_t repetitionPathEntriesScanned = 0;

struct RepetitionPathEntryGuard {
    BitBoard& board;
    bool added;

    ~RepetitionPathEntryGuard() {
        if (added) --board.pathDepth;
    }
};

class SearchDeadlineTimer {
public:
    SearchDeadlineTimer(bool enabled, std::chrono::steady_clock::time_point deadline)
        : thread_(enabled ? std::thread([this, deadline] {
              std::unique_lock<std::mutex> lock(mutex_);
              if (!condition_.wait_until(lock, deadline, [this] { return cancelled_; }))
                  searchStoppedByTime.store(true, std::memory_order_relaxed);
          }) : std::thread()) {}

    ~SearchDeadlineTimer() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cancelled_ = true;
        }
        condition_.notify_one();
        if (thread_.joinable()) thread_.join();
    }

    SearchDeadlineTimer(const SearchDeadlineTimer&) = delete;
    SearchDeadlineTimer& operator=(const SearchDeadlineTimer&) = delete;

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool cancelled_ = false;
    std::thread thread_;
};

int squareForPieceSquareTable(Piece piece, int square) {
    if ((piece & 7) == 0 || piece == NONE) return square;

    // The reference's move-ordering tables use white tables directly and
    // vertically flipped black tables. Its square indexing starts at a1,
    // while Ironfang starts at a8, so the conversion is the inverse of what
    // static evaluation uses for each side.
    return getPieceColor(piece) == WHITE ? Evaluation::mirror(square) : square;
}

int referenceMoveFlag(const Move& move) {
    if (move.isEnPassant) return 1;
    if (move.isKingSideCastle || move.isQueenSideCastle) return 2;
    switch (move.promotion & 7) {
        case 5: return 4;
        case 2: return 5;
        case 4: return 6;
        case 3: return 7;
    }
    if (move.piece == TT_PAWN_DOUBLE_PUSH_HINT ||
        ((move.piece & 7) == 1 && std::abs(move.to - move.from) == 16))
        return 3;
    return 0;
}

Gamestate saveGameState(const BitBoard& board) {
    return {
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
}

struct ReferenceAttackData {
    bool inCheck = false;
    bool inDoubleCheck = false;
    uint64_t checkRay = 0;
    uint64_t pinRays = 0;
    uint64_t opponentSlidingAttacks = 0;
    uint64_t opponentAttacksNoPawns = 0;
    uint64_t opponentPawnAttacks = 0;
};

uint64_t shiftBits(uint64_t bits, int offset) {
    return offset > 0 ? bits << offset : bits >> -offset;
}

uint64_t reverseBitboardRanks(uint64_t bits) {
    return ((bits & 0x00000000000000FFULL) << 56) |
           ((bits & 0x000000000000FF00ULL) << 40) |
           ((bits & 0x0000000000FF0000ULL) << 24) |
           ((bits & 0x00000000FF000000ULL) << 8)  |
           ((bits & 0x000000FF00000000ULL) >> 8)  |
           ((bits & 0x0000FF0000000000ULL) >> 24) |
           ((bits & 0x00FF000000000000ULL) >> 40) |
           ((bits & 0xFF00000000000000ULL) >> 56);
}

int popReferenceLSB(uint64_t& bits) {
    const int referenceSquare = __builtin_ctzll(reverseBitboardRanks(bits));
    const int internalSquare = referenceSquare ^ 56;
    bits &= ~(1ULL << internalSquare);
    return internalSquare;
}

bool squareIsPinnedToKing(const ReferenceAttackData& attacks, int square) {
    return (attacks.pinRays & (1ULL << square)) != 0;
}

bool targetRemainsOnPinLine(int kingSquare, int fromSquare, int targetSquare) {
    const int kingRank = kingSquare / 8;
    const int kingFile = kingSquare & 7;
    const int fromRankDelta = fromSquare / 8 - kingRank;
    const int fromFileDelta = (fromSquare & 7) - kingFile;
    const int targetRankDelta = targetSquare / 8 - kingRank;
    const int targetFileDelta = (targetSquare & 7) - kingFile;

    if (fromRankDelta == 0) return targetRankDelta == 0;
    if (fromFileDelta == 0) return targetFileDelta == 0;
    if (std::abs(fromRankDelta) != std::abs(fromFileDelta)) return false;
    return fromRankDelta * targetFileDelta == fromFileDelta * targetRankDelta;
}

void calculateReferenceAttackData(const BitBoard& board, ReferenceAttackData& attacks) {
    attacks = {};
    const Color mover = board.sideToMove;
    const Color opponent = mover == WHITE ? BLACK : WHITE;
    const int kingSquare = mover == WHITE ? board.whiteKingSquare : board.blackKingSquare;
    const int kingRank = kingSquare / 8;
    const int kingFile = kingSquare & 7;
    const uint64_t opponentBishops = opponent == WHITE
        ? board.getWhiteBishops() : board.getBlackBishops();
    const uint64_t opponentRooks = opponent == WHITE
        ? board.getWhiteRooks() : board.getBlackRooks();
    const uint64_t opponentQueens = opponent == WHITE
        ? board.getWhiteQueens() : board.getBlackQueens();
    const uint64_t opponentKnights = opponent == WHITE
        ? board.getWhiteKnights() : board.getBlackKnights();
    const uint64_t opponentPawns = opponent == WHITE
        ? board.getWhitePawns() : board.getBlackPawns();
    const uint64_t allPieces = board.getAllPieces();
    const uint64_t blockersWithoutKing = allPieces & ~(1ULL << kingSquare);

    uint64_t sliders = opponentRooks | opponentQueens;
    while (sliders)
        attacks.opponentSlidingAttacks |= board.getRookAttacks(
            board.popLSB(sliders), blockersWithoutKing);
    sliders = opponentBishops | opponentQueens;
    while (sliders)
        attacks.opponentSlidingAttacks |= board.getBishopAttacks(
            board.popLSB(sliders), blockersWithoutKing);

    // Find sliding checks and pins by walking outward from the friendly king.
    constexpr int rankStep[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
    constexpr int fileStep[8] = {0, 0, -1, 1, -1, 1, -1, 1};
    for (int direction = 0; direction < 8; ++direction) {
        const bool diagonal = direction >= 4;
        bool hasFriendlyBlocker = false;
        uint64_t ray = 0;
        int rank = kingRank;
        int file = kingFile;
        while (true) {
            rank += rankStep[direction];
            file += fileStep[direction];
            if (rank < 0 || rank >= 8 || file < 0 || file >= 8) break;
            const int square = rank * 8 + file;
            ray |= 1ULL << square;
            const Piece piece = board.getPiece(square);
            if (piece == NONE) continue;
            if (getPieceColor(piece) == mover) {
                if (!hasFriendlyBlocker) {
                    hasFriendlyBlocker = true;
                    continue;
                }
                break;
            }

            const int type = piece & 7;
            const bool matchingSlider = diagonal
                ? (type == 3 || type == 5)
                : (type == 4 || type == 5);
            if (matchingSlider) {
                if (hasFriendlyBlocker) {
                    attacks.pinRays |= ray;
                } else {
                    if (attacks.inCheck) attacks.inDoubleCheck = true;
                    attacks.inCheck = true;
                    attacks.checkRay |= ray;
                }
            }
            break;
        }
        if (attacks.inDoubleCheck) break;
    }

    uint64_t knightAttacks = 0;
    uint64_t knights = opponentKnights;
    while (knights) {
        const int square = board.popLSB(knights);
        const uint64_t moves = board.getKnightAttacks(square);
        knightAttacks |= moves;
        if (moves & (1ULL << kingSquare)) {
            if (attacks.inCheck) attacks.inDoubleCheck = true;
            attacks.inCheck = true;
            attacks.checkRay |= 1ULL << square;
        }
    }

    uint64_t kingAttacks = board.getKingAttacks(
        opponent == WHITE ? board.whiteKingSquare : board.blackKingSquare);
    uint64_t pawns = opponentPawns;
    while (pawns) {
        const int square = board.popLSB(pawns);
        const uint64_t pawnAttacks = board.getPawnAttacks(square, opponent);
        attacks.opponentPawnAttacks |= pawnAttacks;
        if (pawnAttacks & (1ULL << kingSquare)) {
            if (attacks.inCheck) attacks.inDoubleCheck = true;
            attacks.inCheck = true;
            attacks.checkRay |= 1ULL << square;
        }
    }

    attacks.opponentAttacksNoPawns = attacks.opponentSlidingAttacks |
                                     knightAttacks | kingAttacks;
    if (!attacks.inCheck) attacks.checkRay = ~0ULL;
}

template <typename Output>
void addReferenceMove(const BitBoard& board, Output& moves, Piece piece,
                      int from, int to, int promotion = 0,
                      bool kingSideCastle = false, bool queenSideCastle = false,
                      bool enPassant = false) {
    const int capture = enPassant ? NONE : board.getPiece(to);
    if constexpr (std::is_same_v<typename Output::value_type, SearchMove>) {
        int flag = 0;
        if (enPassant) flag = 1;
        else if (kingSideCastle || queenSideCastle) flag = 2;
        else if (promotion) {
            switch (promotion & 7) {
                case 5: flag = 4; break;
                case 2: flag = 5; break;
                case 4: flag = 6; break;
                case 3: flag = 7; break;
            }
        } else if ((piece & 7) == 1 && std::abs(to - from) == 16) {
            flag = 3;
        }
        moves.push_back(SearchMove(static_cast<uint16_t>(
            from | (to << 6) | (flag << 12))));
    } else {
        moves.emplace_back(piece, from, to, promotion, capture,
                           kingSideCastle, queenSideCastle, enPassant);
    }
}

template <typename Output>
void generateReferenceMoves(const BitBoard& board, Output& moves,
                            bool capturesOnly, ReferenceAttackData& attacks) {
    moves.clear();
    if constexpr (std::is_same_v<typename Output::value_type, Move>)
        moves.reserve(218);
    calculateReferenceAttackData(board, attacks);

    const Color mover = board.sideToMove;
    const Color opponent = mover == WHITE ? BLACK : WHITE;
    const bool generateQuietMoves = !capturesOnly;
    const int kingSquare = mover == WHITE ? board.whiteKingSquare : board.blackKingSquare;
    const uint64_t friendlyPieces = mover == WHITE
        ? board.getWhitePieces() : board.getBlackPieces();
    const uint64_t enemyPieces = mover == WHITE
        ? board.getBlackPieces() : board.getWhitePieces();
    const uint64_t allPieces = board.getAllPieces();
    const uint64_t emptySquares = ~allPieces;
    const uint64_t moveTypeMask = generateQuietMoves ? ~0ULL : enemyPieces;
    const uint64_t moveMask = ~friendlyPieces & attacks.checkRay & moveTypeMask;

    uint64_t kingMoves = board.getKingAttacks(kingSquare) &
        ~(attacks.opponentAttacksNoPawns | attacks.opponentPawnAttacks | friendlyPieces) &
        moveTypeMask;
    while (kingMoves) {
        const int target = popReferenceLSB(kingMoves);
        addReferenceMove(board, moves, mover == WHITE ? WK : BK, kingSquare, target);
    }

    if (!attacks.inCheck && generateQuietMoves) {
        const uint64_t blockers = allPieces | attacks.opponentAttacksNoPawns |
                                  attacks.opponentPawnAttacks;
        if (mover == WHITE && board.whiteKingsideCastle &&
            !(blockers & ((1ULL << 61) | (1ULL << 62)))) {
            addReferenceMove(board, moves, WK, kingSquare, 62, 0, true);
        } else if (mover == BLACK && board.blackKingsideCastle &&
                   !(blockers & ((1ULL << 5) | (1ULL << 6)))) {
            addReferenceMove(board, moves, BK, kingSquare, 6, 0, true);
        }
        const uint64_t queensidePath = mover == WHITE
            ? ((1ULL << 58) | (1ULL << 59)) : ((1ULL << 2) | (1ULL << 3));
        const uint64_t queensideEmpty = mover == WHITE
            ? (1ULL << 57) : (1ULL << 1);
        const bool canCastleQueenside = mover == WHITE
            ? board.whiteQueensideCastle : board.blackQueensideCastle;
        if (canCastleQueenside && !(blockers & queensidePath) &&
            !(allPieces & queensideEmpty)) {
            addReferenceMove(board, moves, mover == WHITE ? WK : BK, kingSquare,
                             mover == WHITE ? 58 : 2, 0, false, true);
        }
    }

    if (attacks.inDoubleCheck) return;

    uint64_t orthogonalSliders = mover == WHITE
        ? (board.getWhiteRooks() | board.getWhiteQueens())
        : (board.getBlackRooks() | board.getBlackQueens());
    uint64_t diagonalSliders = mover == WHITE
        ? (board.getWhiteBishops() | board.getWhiteQueens())
        : (board.getBlackBishops() | board.getBlackQueens());
    if (attacks.inCheck) {
        orthogonalSliders &= ~attacks.pinRays;
        diagonalSliders &= ~attacks.pinRays;
    }

    while (orthogonalSliders) {
        const int from = popReferenceLSB(orthogonalSliders);
        uint64_t destinations = board.getRookAttacks(from, allPieces) & moveMask;
        if (squareIsPinnedToKing(attacks, from)) {
            uint64_t legalDestinations = 0;
            uint64_t candidates = destinations;
            while (candidates) {
                const int target = popReferenceLSB(candidates);
                if (targetRemainsOnPinLine(kingSquare, from, target))
                    legalDestinations |= 1ULL << target;
            }
            destinations = legalDestinations;
        }
        const Piece piece = board.getPiece(from);
        while (destinations)
            addReferenceMove(board, moves, piece, from, popReferenceLSB(destinations));
    }

    while (diagonalSliders) {
        const int from = popReferenceLSB(diagonalSliders);
        uint64_t destinations = board.getBishopAttacks(from, allPieces) & moveMask;
        if (squareIsPinnedToKing(attacks, from)) {
            uint64_t legalDestinations = 0;
            uint64_t candidates = destinations;
            while (candidates) {
                const int target = popReferenceLSB(candidates);
                if (targetRemainsOnPinLine(kingSquare, from, target))
                    legalDestinations |= 1ULL << target;
            }
            destinations = legalDestinations;
        }
        const Piece piece = board.getPiece(from);
        while (destinations)
            addReferenceMove(board, moves, piece, from, popReferenceLSB(destinations));
    }

    uint64_t knights = mover == WHITE
        ? board.getWhiteKnights() : board.getBlackKnights();
    knights &= ~attacks.pinRays;
    while (knights) {
        const int from = popReferenceLSB(knights);
        uint64_t destinations = board.getKnightAttacks(from) & moveMask;
        while (destinations)
            addReferenceMove(board, moves, mover == WHITE ? WN : BN,
                             from, popReferenceLSB(destinations));
    }

    uint64_t pawns = mover == WHITE ? board.getWhitePawns() : board.getBlackPawns();
    const Piece pawn = mover == WHITE ? WP : BP;
    const int forward = mover == WHITE ? -8 : 8;
    const uint64_t promotionRank = mover == WHITE
        ? 0x00000000000000FFULL : 0xFF00000000000000ULL;
    const uint64_t doublePushRank = mover == WHITE
        ? 0x000000FF00000000ULL : 0x00000000FF000000ULL;
    const uint64_t singlePush = shiftBits(pawns, forward) & emptySquares;
    uint64_t pushPromotions = singlePush & promotionRank & attacks.checkRay;
    uint64_t singlePushNoPromotions = singlePush & ~promotionRank & attacks.checkRay;
    if (generateQuietMoves) {
        while (singlePushNoPromotions) {
            const int target = popReferenceLSB(singlePushNoPromotions);
            const int from = target - forward;
            if (!squareIsPinnedToKing(attacks, from) ||
                targetRemainsOnPinLine(kingSquare, from, target))
                addReferenceMove(board, moves, pawn, from, target);
        }

        uint64_t doublePush = shiftBits(singlePush, forward) & emptySquares &
                              doublePushRank & attacks.checkRay;
        while (doublePush) {
            const int target = popReferenceLSB(doublePush);
            const int from = target - forward * 2;
            if (!squareIsPinnedToKing(attacks, from) ||
                targetRemainsOnPinLine(kingSquare, from, target))
                addReferenceMove(board, moves, pawn, from, target);
        }
    }

    const uint64_t fileA = 0x0101010101010101ULL;
    const uint64_t fileH = 0x8080808080808080ULL;
    const int captureAOffset = mover == WHITE ? -9 : 9;
    const int captureBOffset = mover == WHITE ? -7 : 7;
    const uint64_t captureASourceMask = mover == WHITE ? ~fileA : ~fileH;
    const uint64_t captureBSourceMask = mover == WHITE ? ~fileH : ~fileA;
    uint64_t capturePromotionsA = shiftBits(pawns & captureASourceMask, captureAOffset) &
                                  enemyPieces & promotionRank & attacks.checkRay;
    uint64_t capturePromotionsB = shiftBits(pawns & captureBSourceMask, captureBOffset) &
                                  enemyPieces & promotionRank & attacks.checkRay;
    uint64_t capturesA = shiftBits(pawns & captureASourceMask, captureAOffset) & enemyPieces;
    uint64_t capturesB = shiftBits(pawns & captureBSourceMask, captureBOffset) & enemyPieces;
    capturesA &= attacks.checkRay & ~promotionRank;
    capturesB &= attacks.checkRay & ~promotionRank;

    while (capturesA) {
        const int target = popReferenceLSB(capturesA);
        const int from = target - captureAOffset;
        if (!squareIsPinnedToKing(attacks, from) ||
            targetRemainsOnPinLine(kingSquare, from, target))
            addReferenceMove(board, moves, pawn, from, target);
    }
    while (capturesB) {
        const int target = popReferenceLSB(capturesB);
        const int from = target - captureBOffset;
        if (!squareIsPinnedToKing(attacks, from) ||
            targetRemainsOnPinLine(kingSquare, from, target))
            addReferenceMove(board, moves, pawn, from, target);
    }

    auto addPromotions = [&](int from, int to) {
        addReferenceMove(board, moves, pawn, from, to, mover == WHITE ? WQ : BQ);
        if (generateQuietMoves)
            addReferenceMove(board, moves, pawn, from, to, mover == WHITE ? WN : BN);
    };
    while (pushPromotions) {
        const int target = popReferenceLSB(pushPromotions);
        const int from = target - forward;
        if (!squareIsPinnedToKing(attacks, from)) addPromotions(from, target);
    }
    while (capturePromotionsA) {
        const int target = popReferenceLSB(capturePromotionsA);
        const int from = target - captureAOffset;
        if (!squareIsPinnedToKing(attacks, from) ||
            targetRemainsOnPinLine(kingSquare, from, target)) addPromotions(from, target);
    }
    while (capturePromotionsB) {
        const int target = popReferenceLSB(capturePromotionsB);
        const int from = target - captureBOffset;
        if (!squareIsPinnedToKing(attacks, from) ||
            targetRemainsOnPinLine(kingSquare, from, target)) addPromotions(from, target);
    }

    if (board.enPassantSquare >= 0) {
        const int target = board.enPassantSquare;
        const int capturedPawnSquare = target - forward;
        if (attacks.checkRay & (1ULL << capturedPawnSquare)) {
            uint64_t pawnsThatCanCapture = pawns;
            while (pawnsThatCanCapture) {
                const int from = popReferenceLSB(pawnsThatCanCapture);
                if (!(board.getPawnAttacks(from, mover) & (1ULL << target))) continue;
                if (squareIsPinnedToKing(attacks, from) &&
                    !targetRemainsOnPinLine(kingSquare, from, target)) continue;

                const uint64_t enemyOrthogonal =
                    (opponent == WHITE ? board.getWhiteRooks() : board.getBlackRooks()) |
                    (opponent == WHITE ? board.getWhiteQueens() : board.getBlackQueens());
                if (enemyOrthogonal) {
                    // Model the reference's XOR occupancy update: the source
                    // and captured pawns are removed, and the currently empty
                    // EP destination is added as a blocker.
                    const uint64_t blockers =
                        (allPieces & ~((1ULL << capturedPawnSquare) | (1ULL << from))) |
                        (1ULL << target);
                    const uint64_t rookAttacks = board.getRookAttacks(kingSquare, blockers);
                    if (rookAttacks & enemyOrthogonal) continue;
                }
                addReferenceMove(board, moves, pawn, from, target, 0, false, false, true);
            }
        }
    }
}

int partitionMoves(SearchMove* moves, int* scores, int low, int high) {
    const int pivotScore = scores[high];
    int i = low - 1;
    for (int j = low; j <= high - 1; ++j) {
        if (scores[j] > pivotScore) {
            ++i;
            std::swap(moves[i], moves[j]);
            std::swap(scores[i], scores[j]);
        }
    }
    std::swap(moves[i + 1], moves[high]);
    std::swap(scores[i + 1], scores[high]);
    return i + 1;
}

void quicksortMoves(SearchMove* moves, int* scores, int low, int high) {
    if (low < high) {
        const int pivot = partitionMoves(moves, scores, low, high);
        quicksortMoves(moves, scores, low, pivot - 1);
        quicksortMoves(moves, scores, pivot + 1, high);
    }
}

int countCurrentRepetitionOccurrences(const BitBoard& board) {
    // A pawn move or capture makes every earlier position unreachable, and
    // Zobrist identity includes side to move. Only inspect same-side positions
    // within the reversible-move window instead of rescanning the full game.
    // A repeated position needs at least four reversible plies: two moves by
    // each side to return the board and side-to-move to their original state.
    if (board.halfmoveClock < 4) return 0;

    const int currentIndex = board.pathDepth - 1;
    const int earliestRelevantIndex = std::max(0, currentIndex - board.halfmoveClock);
    int occurrences = 0;
    for (int index = currentIndex - 2; index >= earliestRelevantIndex; index -= 2) {
        ++repetitionPathEntriesScanned;
        if (board.repetitionPath[index] == board.zobristKey) {
            ++occurrences;
            // Match the reference searcher's simplified repetition rule: a
            // prior occurrence is sufficient to score this node as a draw.
            if (occurrences >= 1) break;
        }
    }
    return occurrences;
}

void orderMoves(BitBoard& board, SearchMove* moves, int* scores, int moveCount, uint16_t hashMove,
                const ReferenceAttackData& attacks, bool inQuiescence, int ply) {
    const Color mover = board.sideToMove;
    const size_t sideIndex = static_cast<size_t>(mover);
    const uint64_t opponentPawnAttacks = attacks.opponentPawnAttacks;
    const uint64_t opponentAttacks = attacks.opponentAttacksNoPawns;
    const uint64_t allOpponentAttacks = opponentAttacks | opponentPawnAttacks;

    for (int moveIndex = 0; moveIndex < moveCount; ++moveIndex) {
        SearchMove& searchMove = moves[moveIndex];
        int& moveScore = scores[moveIndex];
        const int from = searchMoveFrom(searchMove);
        const int to = searchMoveTo(searchMove);
        const int flag = searchMoveFlag(searchMove);
        const Piece piece = board.getPiece(from);
        const int movingType = piece & 7;
        const Piece capturedPiece = flag == 1
            ? (piece == WP ? BP : WP) : board.getPiece(to);
        const bool isCapture = flag != 1 && capturedPiece != NONE;
        const bool isPromotion = flag >= 4;
        int score = 0;
        // Match the reference's capture classification, which checks the
        // destination square. It therefore treats en passant as a quiet move.

        if (hashMove == searchMove.value) {
            moveScore = HASH_MOVE_SCORE;
            continue;
        }

        const int movingValue = Evaluation::pieceValue[movingType];
        if (isCapture) {
            const int capturedType = capturedPiece & 7;
            const int captureDelta = Evaluation::pieceValue[capturedType] - movingValue;
            // Keep the reference's capture tiers: good captures before quiet
            // promotions, and losing captures behind killer moves. The old
            // scores put every capture below promotions, even when the capture
            // won material, which can delay critical tactical lines.
            const bool attacked = (allOpponentAttacks & (1ULL << to)) != 0;
            if (attacked) {
                score += (captureDelta >= 0 ? WINNING_CAPTURE_BIAS : LOSING_CAPTURE_BIAS) +
                         captureDelta;
            } else {
                score += WINNING_CAPTURE_BIAS + captureDelta;
            }
        }

        if (movingType == 1) {
            if (!isCapture && isPromotion && flag == 5)
                score += PROMOTION_BIAS;
        } else if (movingType != 6) {
            const int tableIndex = movingType - 1;
            if (tableIndex >= 0 && tableIndex < 5) {
                const int fromSquare = squareForPieceSquareTable(piece, from);
                const int toSquare = squareForPieceSquareTable(piece, to);
                score += PST[tableIndex][toSquare] - PST[tableIndex][fromSquare];
            }

            if (opponentPawnAttacks & (1ULL << to)) {
                score -= 50;
            } else if (opponentAttacks & (1ULL << to)) {
                score -= 25;
            }
        }

        if (!isCapture) {
            if (!inQuiescence && ply >= 0 && ply < MAX_KILLER_PLY) {
                if (searchMove.value == killerMoves[ply][0] ||
                    searchMove.value == killerMoves[ply][1])
                    score += KILLER_MOVE_BIAS;
            }
            score += historyHeuristics[sideIndex][from][to];
        }

        moveScore = score;
    }

    if (moveCount > 0) quicksortMoves(moves, scores, 0, moveCount - 1);
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
    if (!force && (nodeCount & 2047ULL) != 0) {
        return false;
    }
    if (searchStopRequested.load(std::memory_order_relaxed)) {
        searchStoppedByTime.store(true, std::memory_order_relaxed);
    }
    return searchStoppedByTime.load(std::memory_order_relaxed);
}

uint64_t transpositionKey(const BitBoard& board) {
    return board.zobristKey;
}

}

void Search::setRootDiagnostics(bool enabled) {
    rootDiagnosticsEnabled = enabled;
}

std::vector<Move> Search::diagnosticLegalMoves(const BitBoard& board) {
    std::vector<Move> moves;
    ReferenceAttackData attacks;
    generateReferenceMoves(board, moves, false, attacks);
    return moves;
}

void Search::newGame() {
    TT.clear();
    historyHeuristics = {};
    searchStopRequested.store(false, std::memory_order_relaxed);
    for (auto& killersAtPly : killerMoves) {
        killersAtPly[0] = 0;
        killersAtPly[1] = 0;
    }
}

void Search::clearStopRequest() {
    searchStopRequested.store(false, std::memory_order_relaxed);
}

void Search::requestStop() {
    searchStopRequested.store(true, std::memory_order_relaxed);
}

static int scoreToTT(int score, int ply) {
    if (score > MATE_THRESHOLD) return score + ply;
    if (score < -MATE_THRESHOLD) return score - ply;
    return score;
}

int Search::searchChild(BitBoard& board, int childDepth, int alpha, int beta,
                        int ply, int numExtensions, bool reduceMove) {
    if (reduceMove) {
        ++lateMoveReductionCount;
        int score = -minimaxAlphaBeta(board, childDepth - 1, -alpha - 1, -alpha,
                                      ply, numExtensions);
        if (searchStoppedByTime || score <= alpha) return score;

        ++lateMoveResearchCount;
        return -minimaxAlphaBeta(board, childDepth, -beta, -alpha,
                                 ply, numExtensions);
    }

    // The reference searches every non-reduced move with a full alpha-beta
    // window; it does not use PVS.
    return -minimaxAlphaBeta(board, childDepth, -beta, -alpha,
                             ply, numExtensions);
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
    pvsResearchCount = 0;
    nullMoveTryCount = 0;
    nullMoveCutoffCount = 0;
    repetitionPathEntriesScanned = 0;
    pvLength.fill(0);
    previousRootBestMove = 0;
    // The reference clears history before every search but retains killers
    // until ucinewgame.
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
        std::lock_guard<std::mutex> outputLock(uciOutputMutex);
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
                  << " pvs_researches=" << pvsResearchCount
                  << " null_move_tries=" << nullMoveTryCount
                  << " null_move_cutoffs=" << nullMoveCutoffCount
                  << " repetition_scan_entries=" << repetitionPathEntriesScanned;
        if (searchStopRequested.load(std::memory_order_relaxed)) {
            std::cout << " stop=uci";
        } else if (searchStoppedByTime) {
            std::cout << " stop=hard_time";
        }
        std::cout << "\n" << std::flush;
    };
    
    // Search to the requested limit; periodic node checks bound stop latency.
    const int hardTimeLimit = timeLimit;
    hasHardSearchDeadline = hardTimeLimit >= 0;
    if (hasHardSearchDeadline) {
        hardSearchDeadline = startTime + std::chrono::milliseconds(hardTimeLimit);
    }
    SearchDeadlineTimer deadlineTimer(hasHardSearchDeadline, hardSearchDeadline);
    
    // Default move (will be overwritten soon)
    Move bestMove{NONE, -1, -1};
    int bestScore = -INF;
    
    // Generate all  moves
    ReferenceAttackData rootAttackData;
    auto& rootMoves = searchMoveBuffers[0];
    SearchMoveOutput rootMoveOutput{rootMoves.data(), 0};
    generateReferenceMoves(board, rootMoveOutput, false, rootAttackData);

    if (rootMoveOutput.count == 0) {
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

    // Keep a legal fallback ready if the first timed iteration is interrupted.
    bestMove = decodeSearchMove(board, rootMoves[0]);

    std::array<Move, MAX_PLY> bestPV{};
    int bestPVLength = 1;
    bestPV[0] = bestMove;

    if (checkSearchDeadline(true)) {
        emitSearchStats();
        return bestMove;
    }

    // A zero clock budget is distinct from an absent time limit (-1). Keep
    // the legal fallback move instead of treating go wtime 0 as infinite.
    if (timeLimit == 0) {
        emitSearchStats();
        return bestMove;
    }
    
    // ------------------------------------------------------------------------------------
    //                          ITERATIVE DEEPENING SEARCH
    // ------------------------------------------------------------------------------------
    for (int depth = 1; depth <= maxDepth; depth++) {
        selectiveDepth = 0;

        if (depth > 1 && checkSearchDeadline(true)) {
            break;
        }

        // Use the same root Search() path as the reference rather than
        // maintaining a second, hand-copied root implementation here.
        iterationBestMove = Move(NONE, -1, -1);
        iterationBestScore = -INF;
        hasSearchedAtLeastOneRootMove = false;
        const int completedScore = minimaxAlphaBeta(board, depth, -INF, INF, 0, 0);

        if (searchStoppedByTime) {
            if (hasSearchedAtLeastOneRootMove) {
                bestMove = iterationBestMove;
                bestScore = iterationBestScore;
                bestPVLength = std::max(1, std::min(pvLength[0], MAX_PLY));
                for (int index = 0; index < bestPVLength; ++index)
                    bestPV[index] = pvTable[0][index];
            }
            emitSearchStats();
            return bestMove;
        }

        bestMove = iterationBestMove;
        bestScore = completedScore;
        previousRootBestMove = packSearchMove(bestMove);
        bestPVLength = std::max(1, std::min(pvLength[0], MAX_PLY));
        for (int index = 0; index < bestPVLength; ++index)
            bestPV[index] = pvTable[0][index];

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
        std::lock_guard<std::mutex> outputLock(uciOutputMutex);
        std::cout << "info depth " << depth
                  << " seldepth " << selectiveDepth
                  << " time " << elapsedMs
                  << " score ";
        if (bestScore > MATE_THRESHOLD || bestScore < -MATE_THRESHOLD) {
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

        // A proven forced win cannot be improved by searching for a longer
        // line. Stop now so the remaining clock is available for later moves.
        if (std::abs(bestScore) > MATE_THRESHOLD &&
            MATE_SCORE - std::abs(bestScore) <= depth) break;
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

    const bool appendPathPosition = ply > 0 || board.pathDepth == 0 ||
        board.repetitionPath[board.pathDepth - 1] != board.zobristKey;
    if (appendPathPosition) {
        if (board.pathDepth >= 1024) return Evaluation::evaluate(board);
        board.repetitionPath[board.pathDepth++] = board.zobristKey;
    }
    const RepetitionPathEntryGuard pathEntryGuard{board, appendPathPosition};

    if (ply > 0 &&
        (board.halfmoveClock >= 100 || countCurrentRepetitionOccurrences(board) >= 1)) {
        return 0;
    }

    if (ply > 0) {
        alpha = std::max(alpha, -MATE_SCORE + ply);
        beta = std::min(beta, MATE_SCORE - ply);
        if (alpha >= beta) {
            return alpha;
        }
    }

    const int originalAlpha = alpha;
    uint16_t tempPackedMove = 0;
    int tempEval;
    if (TT.probe(transpositionKey(board), depth, alpha, beta, ply, tempEval, tempPackedMove)) {
        if (ply == 0) {
            iterationBestMove = decodeSearchMove(board, SearchMove(tempPackedMove));
            iterationBestScore = tempEval;
            if (tempPackedMove != 0) {
                pvTable[0][0] = iterationBestMove;
                pvLength[0] = 1;
            }
        }
        return tempEval;
    }

    if (depth <= 0) {
        return quiescenceSearch(board, alpha, beta, ply);
    }

    ReferenceAttackData attackData;
    auto& moves = searchMoveBuffers[ply];
    auto& scores = searchMoveScores[ply];
    SearchMoveOutput moveOutput{moves.data(), 0};
    generateReferenceMoves(board, moveOutput, false, attackData);
    const int moveCount = moveOutput.count;
    searchMoveCounts[ply] = moveCount;
    if (moveCount == 0) {
        if (attackData.inCheck) {
            return -MATE_SCORE + ply;
        }
        return 0; // stalemate
    }

    // The reference seeds root ordering with the previous completed iteration's
    // best move. Internal nodes use the move stored at the current TT index,
    // even when the indexed entry belongs to a colliding key.
    const uint16_t moveOrderHint = ply == 0 ? previousRootBestMove : tempPackedMove;
    orderMoves(board, moves.data(), scores.data(), moveCount, moveOrderHint, attackData, false, ply);

    uint16_t bestPackedMove = 0;
    for (int moveIndex = 0; moveIndex < moveCount; ++moveIndex) {
        const uint64_t nodesBefore = nodeCount;
        const uint64_t reductionsBefore = lateMoveReductionCount;
        const uint64_t researchesBefore = lateMoveResearchCount;
        const Gamestate prevdata = saveGameState(board);
        const int from = searchMoveFrom(moves[moveIndex]);
        const int to = searchMoveTo(moves[moveIndex]);
        const int moveFlag = moves[moveIndex].value >> 12;
        const Piece movingPiece = board.getPiece(from);
        Piece capturedPiece = board.getPiece((moves[moveIndex].value >> 6) & 0x3F);
        if (moveFlag == 1)
            capturedPiece = prevdata.sideToMove == WHITE ? BP : WP;
        board.makeSearchMove(moves[moveIndex].value, capturedPiece);

        int extension = 0;
        if (numExtensions < MAX_EXTENSIONS) {
            const bool givesCheck = board.isInCheck();
            const int targetRank = to / 8;
            const bool advancedPawn = (movingPiece & 7) == 1 &&
                (targetRank == 1 || targetRank == 6);
            if (givesCheck || advancedPawn) extension = 1;
        }

        const bool isCapture = capturedPiece != NONE;
        const bool reduceMove = extension == 0 && depth >= 3 &&
            moveIndex >= 3 && !isCapture;

        const int score = searchChild(
            board, depth - 1 + extension, alpha, beta, ply + 1,
            numExtensions + extension, reduceMove);
        board.unmakeSearchMove(moves[moveIndex].value, prevdata, capturedPiece);

        if (searchStoppedByTime || (ply == 0 && checkSearchDeadline(true))) {
            return 0;
        }

        if (rootDiagnosticsEnabled && ply == 0) {
            const Move move = decodeSearchMove(board, moves[moveIndex]);
            std::array<Move, MAX_PLY> candidatePV{};
            int candidatePVLength = 1;
            candidatePV[0] = move;
            const int continuationLength = std::max(
                0, std::min(pvLength[1] - 1, MAX_PLY - 2));
            for (int index = 0; index < continuationLength; ++index)
                candidatePV[index + 1] = pvTable[1][index + 1];
            candidatePVLength += continuationLength;

            std::lock_guard<std::mutex> outputLock(uciOutputMutex);
            std::cout << "info string root depth=" << depth
                      << " move=" << moveToUCI(move)
                      << " score=" << score
                      << " nodes=" << (nodeCount - nodesBefore)
                      << " lmr=" << (lateMoveReductionCount - reductionsBefore)
                      << " lmr_researches=" << (lateMoveResearchCount - researchesBefore)
                      << " pv";
            for (int index = 0; index < candidatePVLength; ++index)
                std::cout << ' ' << moveToUCI(candidatePV[index]);
            std::cout << "\n" << std::flush;
        }

        // Beta cutoff
        if (score >= beta) {
            const Move move = decodeSearchMove(board, moves[moveIndex]);
            ++betaCutoffCount;
            if (moveIndex == 0) ++firstMoveCutoffCount;
            updatePrincipalVariation(ply, move);

            if (!isCapture && ply < MAX_KILLER_PLY &&
                moves[moveIndex].value != killerMoves[ply][0]) {
                killerMoves[ply][1] = killerMoves[ply][0];
                killerMoves[ply][0] = moves[moveIndex].value;
            }
            const size_t sideIndex = static_cast<size_t>(prevdata.sideToMove);
            if (!isCapture && move.from >= 0 && move.from < 64 && move.to >= 0 && move.to < 64) {
                int& history = historyHeuristics[sideIndex][move.from][move.to];
                history += depth * depth;
            }
            TT.store(transpositionKey(board), depth, scoreToTT(beta, ply), TT_LOWER,
                     moves[moveIndex].value);

            return beta;
        }

        if (score > alpha) {
            const Move move = decodeSearchMove(board, moves[moveIndex]);
            alpha = score;
            bestPackedMove = moves[moveIndex].value;
            updatePrincipalVariation(ply, move);
            if (ply == 0) {
                iterationBestMove = move;
                iterationBestScore = score;
                hasSearchedAtLeastOneRootMove = true;
            }
        }
    }

    TT_FLAG flag;
    if(alpha <= originalAlpha) {
        flag = TT_UPPER;
    }
    else {
        flag = TT_EXACT;
    }

    TT.store(transpositionKey(board), depth, scoreToTT(alpha, ply), flag, bestPackedMove);
    return alpha;
}

int Search::quiescenceSearch(BitBoard& board, int alpha, int beta, int ply) {
    ++nodeCount;
    ++quiescenceNodeCount;
    selectiveDepth = std::max(selectiveDepth, ply);
    if (ply < MAX_PLY) pvLength[ply] = ply;

    if (checkSearchDeadline()) return 0;
    if (ply >= MAX_PLY - 1) return Evaluation::evaluate(board);
    const int standPat = Evaluation::evaluate(board);
    if (standPat >= beta) return beta;
    alpha = std::max(alpha, standPat);

    ReferenceAttackData attackData;
    auto& moves = searchMoveBuffers[ply];
    auto& scores = searchMoveScores[ply];
    SearchMoveOutput moveOutput{moves.data(), 0};
    generateReferenceMoves(board, moveOutput, true, attackData);
    const int moveCount = moveOutput.count;
    orderMoves(board, moves.data(), scores.data(), moveCount,
               0, attackData, true, 0);

    for (int moveIndex = 0; moveIndex < moveCount; ++moveIndex) {
        const Gamestate previousState = saveGameState(board);
        const int moveFlag = moves[moveIndex].value >> 12;
        Piece capturedPiece = board.getPiece((moves[moveIndex].value >> 6) & 0x3F);
        if (moveFlag == 1)
            capturedPiece = previousState.sideToMove == WHITE ? BP : WP;
        board.makeSearchMove(moves[moveIndex].value, capturedPiece);
        const int score = -quiescenceSearch(board, -beta, -alpha, ply + 1);
        board.unmakeSearchMove(moves[moveIndex].value, previousState, capturedPiece);

        if (searchStoppedByTime) return 0;
        if (score >= beta) {
            const Move move = decodeSearchMove(board, moves[moveIndex]);
            updatePrincipalVariation(ply, move);
            return beta;
        }
        if (score > alpha) {
            const Move move = decodeSearchMove(board, moves[moveIndex]);
            alpha = score;
            updatePrincipalVariation(ply, move);
        }
    }

    return alpha;
}
