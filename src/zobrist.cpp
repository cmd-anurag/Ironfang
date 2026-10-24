#include "zobrist.h"
#include "bitboard.h"
#include <array>
#include <cstdint>

uint64_t zobristPieces[12][64];
uint64_t zobristCastling[16];
uint64_t zobristEnPassant[9];
uint64_t zobristBlackToMove;

namespace {
// System.Random's seeded compatibility generator, used by the reference
// engine to create its deterministic Zobrist values.
class ReferenceRandom {
public:
    explicit ReferenceRandom(int seed) {
        constexpr int big = 2147483647;
        constexpr int seedConstant = 161803398;
        const int subtraction = seed == (-2147483647 - 1)
            ? 2147483647 : (seed < 0 ? -seed : seed);
        int mj = seedConstant - subtraction;
        if (mj < 0) mj += big;
        seedArray[55] = mj;
        int mk = 1;
        for (int i = 1; i < 55; ++i) {
            const int ii = (21 * i) % 55;
            seedArray[ii] = mk;
            mk = mj - mk;
            if (mk < 0) mk += big;
            mj = seedArray[ii];
        }
        for (int k = 1; k < 5; ++k) {
            for (int i = 1; i < 56; ++i) {
                seedArray[i] -= seedArray[1 + (i + 30) % 55];
                if (seedArray[i] < 0) seedArray[i] += big;
            }
        }
    }

    uint64_t nextUInt64() {
        std::array<uint8_t, 8> bytes{};
        for (uint8_t& byte : bytes)
            byte = static_cast<uint8_t>(internalSample() % 256);
        uint64_t value = 0;
        for (size_t i = 0; i < bytes.size(); ++i)
            value |= static_cast<uint64_t>(bytes[i]) << (i * 8);
        return value;
    }

private:
    int internalSample() {
        constexpr int big = 2147483647;
        if (++inext >= 56) inext = 1;
        if (++inextp >= 56) inextp = 1;
        int result = seedArray[inext] - seedArray[inextp];
        if (result == big) --result;
        if (result < 0) result += big;
        seedArray[inext] = result;
        return result;
    }

    std::array<int, 56> seedArray{};
    int inext = 0;
    int inextp = 21;
};

int castlingIndex(const BitBoard& board) {
    return castlingRightsIndex(board.whiteKingsideCastle, board.whiteQueensideCastle,
                               board.blackKingsideCastle, board.blackQueensideCastle);
}
}

void initZobrist() {
    ReferenceRandom rng(29426028);
    // The reference fills piece keys square-first, then in Piece.PieceIndices
    // order (white P/N/B/R/Q/K followed by black P/N/B/R/Q/K).
    for (int referenceSquare = 0; referenceSquare < 64; ++referenceSquare) {
        for (int piece = 0; piece < 12; ++piece)
            zobristPieces[piece][referenceSquare] = rng.nextUInt64();
    }
    for (uint64_t& key : zobristCastling) key = rng.nextUInt64();
    zobristEnPassant[0] = 0;
    for (int file = 1; file <= 8; ++file)
        zobristEnPassant[file] = rng.nextUInt64();
    zobristBlackToMove = rng.nextUInt64();
}

uint64_t generateZobristHashKey(BitBoard& board) {
    uint64_t key = 0;
    for (int square = 0; square < 64; ++square) {
        const int index = pieceToZobristIndex(board.getPiece(square));
        if (index >= 0) key ^= zobristPieces[index][square ^ 56];
    }
    key ^= zobristCastling[castlingIndex(board)];
    const int epFile = board.enPassantSquare < 0 ? 0 : (board.enPassantSquare & 7) + 1;
    key ^= zobristEnPassant[epFile];
    if (board.sideToMove == BLACK) key ^= zobristBlackToMove;
    return key;
}

uint64_t generateZobristHashKey(Board& board) {
    uint64_t key = 0;
    for (int square = 0; square < 64; ++square) {
        const int index = pieceToZobristIndex(board.getPiece(square));
        if (index >= 0) key ^= zobristPieces[index][square ^ 56];
    }
    key ^= zobristCastling[castlingRightsIndex(
        board.whiteKingsideCastle, board.whiteQueensideCastle,
        board.blackKingsideCastle, board.blackQueensideCastle)];
    const int epFile = board.enPassantSquare < 0 ? 0 : (board.enPassantSquare & 7) + 1;
    key ^= zobristEnPassant[epFile];
    if (board.sideToMove == BLACK) key ^= zobristBlackToMove;
    return key;
}
