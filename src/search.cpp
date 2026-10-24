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

std::vector<std::vector<Move>> killerMoves(MAX_PLY, std::vector<Move>(2, Move(NONE, -1, -1)));
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
std::array<std::array<Move, MAX_PLY>, MAX_PLY> pvTable{};
std::array<int, MAX_PLY> pvLength{};
std::array<std::vector<Move>, MAX_PLY> searchMoveBuffers;
std::chrono::steady_clock::time_point hardSearchDeadline;
bool hasHardSearchDeadline = false;
std::atomic<bool> searchStoppedByTime{false};
std::atomic<bool> searchStopRequested{false};
bool rootDiagnosticsEnabled = false;
Move iterationBestMove(NONE, -1, -1);
int iterationBestScore = -INF;
bool hasSearchedAtLeastOneRootMove = false;
Move previousRootBestMove(NONE, -1, -1);
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

bool matchesHashMove(const Move& move, const Move& hashMove) {
    return hashMove.from >= 0 && move.from == hashMove.from &&
        move.to == hashMove.to && referenceMoveFlag(move) == referenceMoveFlag(hashMove);
}

bool matchesReferenceMoveIdentity(const Move& move, const Move& other) {
    return move.from == other.from && move.to == other.to &&
        move.from >= 0 && move.to >= 0 &&
        referenceMoveFlag(move) == referenceMoveFlag(other);
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

void addReferenceMove(const BitBoard& board, std::vector<Move>& moves, Piece piece,
                      int from, int to, int promotion = 0,
                      bool kingSideCastle = false, bool queenSideCastle = false,
                      bool enPassant = false) {
    const int capture = enPassant ? NONE : board.getPiece(to);
    moves.emplace_back(piece, from, to, promotion, capture,
                       kingSideCastle, queenSideCastle, enPassant);
}

void generateReferenceMoves(const BitBoard& board, std::vector<Move>& moves,
                            bool capturesOnly, ReferenceAttackData& attacks) {
    moves.clear();
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

int moveScore(const Move& move) {
    return move.heuristicScore;
}

int partitionMoves(std::vector<Move>& moves, int low, int high) {
    const int pivotScore = moveScore(moves[high]);
    int i = low - 1;
    for (int j = low; j <= high - 1; ++j) {
        if (moveScore(moves[j]) > pivotScore) {
            ++i;
            std::swap(moves[i], moves[j]);
        }
    }
    std::swap(moves[i + 1], moves[high]);
    return i + 1;
}

void quicksortMoves(std::vector<Move>& moves, int low, int high) {
    if (low < high) {
        const int pivot = partitionMoves(moves, low, high);
        quicksortMoves(moves, low, pivot - 1);
        quicksortMoves(moves, pivot + 1, high);
    }
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
            if (occurrences >= 2) break;
        }
    }
    return occurrences;
}

void orderMoves(BitBoard& board, std::vector<Move>& moves, const Move& hashMove,
                const ReferenceAttackData& attacks, bool inQuiescence, int ply) {
    const Color mover = board.sideToMove;
    const size_t sideIndex = static_cast<size_t>(mover);
    const uint64_t opponentPawnAttacks = attacks.opponentPawnAttacks;
    const uint64_t opponentAttacks = attacks.opponentAttacksNoPawns;
    const uint64_t allOpponentAttacks = opponentAttacks | opponentPawnAttacks;

    for (Move& move : moves) {
        int score = 0;
        // Match the reference's capture classification, which checks the
        // destination square. It therefore treats en passant as a quiet move.
        const bool isCapture = move.capture != NONE;

        if (matchesHashMove(move, hashMove)) {
            move.heuristicScore = HASH_MOVE_SCORE;
            continue;
        }

        const int movingType = move.piece & 7;
        const int movingValue = Evaluation::pieceValue[movingType];
        if (isCapture) {
            const Piece capturedPiece = static_cast<Piece>(move.capture);
            const int capturedType = capturedPiece & 7;
            const int captureDelta = Evaluation::pieceValue[capturedType] - movingValue;
            // Keep the reference's capture tiers: good captures before quiet
            // promotions, and losing captures behind killer moves. The old
            // scores put every capture below promotions, even when the capture
            // won material, which can delay critical tactical lines.
            const bool attacked = (allOpponentAttacks & (1ULL << move.to)) != 0;
            if (attacked) {
                score += (captureDelta >= 0 ? WINNING_CAPTURE_BIAS : LOSING_CAPTURE_BIAS) +
                         captureDelta;
            } else {
                score += WINNING_CAPTURE_BIAS + captureDelta;
            }
        }

        if (movingType == 1) {
            if (!isCapture && (move.promotion & 7) == 5)
                score += PROMOTION_BIAS;
        } else if (movingType != 6) {
            const int tableIndex = movingType - 1;
            if (tableIndex >= 0 && tableIndex < 5) {
                const int fromSquare = squareForPieceSquareTable(move.piece, move.from);
                const int toSquare = squareForPieceSquareTable(move.piece, move.to);
                score += PST[tableIndex][toSquare] - PST[tableIndex][fromSquare];
            }

            if (opponentPawnAttacks & (1ULL << move.to)) {
                score -= 50;
            } else if (opponentAttacks & (1ULL << move.to)) {
                score -= 25;
            }
        }

        if (!isCapture) {
            if (!inQuiescence && ply >= 0 && ply < MAX_KILLER_PLY) {
                if (matchesReferenceMoveIdentity(move, killerMoves[ply][0]) ||
                    matchesReferenceMoveIdentity(move, killerMoves[ply][1]))
                    score += KILLER_MOVE_BIAS;
            }
            if (move.from >= 0 && move.from < 64 && move.to >= 0 && move.to < 64)
                score += historyHeuristics[sideIndex][move.from][move.to];
        }

        move.heuristicScore = score;
    }

    if (!moves.empty()) quicksortMoves(moves, 0, static_cast<int>(moves.size()) - 1);
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

bool checkSearchDeadline(bool /*force*/ = false) {
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
        killersAtPly[0] = Move(NONE, -1, -1);
        killersAtPly[1] = Move(NONE, -1, -1);
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
    previousRootBestMove = Move(NONE, -1, -1);
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
    std::vector<Move> moves;
    generateReferenceMoves(board, moves, false, rootAttackData);
    
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

    // Keep a legal fallback ready if the first timed iteration is interrupted.
    bestMove = moves.front();

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
        previousRootBestMove = bestMove;
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
        (board.halfmoveClock >= 100 || countCurrentRepetitionOccurrences(board) >= 2)) {
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
    Move tempMove = Move(NONE, -1, -1);
    int tempEval;
    if (TT.probe(transpositionKey(board), depth, alpha, beta, ply, tempEval, tempMove)) {
        if (ply == 0) {
            iterationBestMove = tempMove;
            iterationBestScore = tempEval;
            if (tempMove.from >= 0) {
                pvTable[0][0] = tempMove;
                pvLength[0] = 1;
            }
        }
        return tempEval;
    }

    if (depth <= 0) {
        return quiescenceSearch(board, alpha, beta, ply);
    }

    std::vector<Move>& moves = searchMoveBuffers[ply];
    ReferenceAttackData attackData;
    generateReferenceMoves(board, moves, false, attackData);
    if (moves.empty()) {
        if (attackData.inCheck) {
            return -MATE_SCORE + ply;
        }
        return 0; // stalemate
    }

    // The reference seeds root ordering with the previous completed iteration's
    // best move. Internal nodes use the move stored at the current TT index,
    // even when the indexed entry belongs to a colliding key.
    const Move& moveOrderHint = ply == 0 ? previousRootBestMove : tempMove;
    orderMoves(board, moves, moveOrderHint, attackData, false, ply);

    Move bestMove(NONE, -1, -1);
    for (size_t moveIndex = 0; moveIndex < moves.size(); ++moveIndex) {
        const Move& move = moves[moveIndex];
        const uint64_t nodesBefore = nodeCount;
        const uint64_t reductionsBefore = lateMoveReductionCount;
        const uint64_t researchesBefore = lateMoveResearchCount;
        const Gamestate prevdata = saveGameState(board);
        board.makeLegalMove(move);

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

        const bool isCapture = move.capture != NONE;
        const bool reduceMove = extension == 0 && depth >= 3 &&
            moveIndex >= 3 && !isCapture;

        const int score = searchChild(
            board, depth - 1 + extension, alpha, beta, ply + 1,
            numExtensions + extension, reduceMove);
        board.unmakeMove(move, prevdata);

        if (searchStoppedByTime || (ply == 0 && checkSearchDeadline(true))) {
            return 0;
        }

        if (rootDiagnosticsEnabled && ply == 0) {
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
            ++betaCutoffCount;
            if (moveIndex == 0) ++firstMoveCutoffCount;
            updatePrincipalVariation(ply, move);

            if (!isCapture && ply < MAX_KILLER_PLY &&
                !matchesReferenceMoveIdentity(move, killerMoves[ply][0])) {
                killerMoves[ply][1] = killerMoves[ply][0];
                killerMoves[ply][0] = move;
            }
            const size_t sideIndex = static_cast<size_t>(prevdata.sideToMove);
            if (!isCapture && move.from >= 0 && move.from < 64 && move.to >= 0 && move.to < 64) {
                int& history = historyHeuristics[sideIndex][move.from][move.to];
                history += depth * depth;
            }
            TT.store(transpositionKey(board), depth, scoreToTT(beta, ply), TT_LOWER, move);

            return beta;
        }

        if (score > alpha) {
            alpha = score;
            bestMove = move;
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

    TT.store(transpositionKey(board), depth, scoreToTT(alpha, ply), flag, bestMove);
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

    std::vector<Move>& moves = searchMoveBuffers[ply];
    ReferenceAttackData attackData;
    generateReferenceMoves(board, moves, true, attackData);
    orderMoves(board, moves, Move(NONE, -1, -1), attackData, true, 0);

    for (const Move& move : moves) {
        const Gamestate previousState = saveGameState(board);
        board.makeLegalMove(move);
        const int score = -quiescenceSearch(board, -beta, -alpha, ply + 1);
        board.unmakeMove(move, previousState);

        if (searchStoppedByTime) return 0;
        if (score >= beta) {
            updatePrincipalVariation(ply, move);
            return beta;
        }
        if (score > alpha) {
            alpha = score;
            updatePrincipalVariation(ply, move);
        }
    }

    return alpha;
}
