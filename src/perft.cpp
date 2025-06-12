#include "perft.h"

uint64_t perft(BitBoard& board, int depth) {
    if (depth == 0) return 1;

    uint64_t nodes = 0;
    std::vector<Move> moves = board.generateMoves();

    for (Move move : moves) {
        Gamestate prevState = {
            board.sideToMove, 
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare,
            board.zobristKey
        };

        if(!board.makeMove(move)) continue;

        nodes += perft(board, depth - 1);
        board.unmakeMove(move, prevState);
    }
    return nodes;
}