// search.cpp
#include "search.h"
#include "evaluate.h"

constexpr int INF = 1000000;
constexpr int MAX_DEPTH = 5;

Move Search::findBestMove(Board& board, int depth) {
    std::vector<Move> moves = board.generateMoves();

    // no pseudo‐legal moves at all, immediate stalemate or checkmate.
    if (moves.empty()) {
        return Move{NONE, -1, -1 };
    }

    Move bestMove{NONE, -1, -1 };
    int   bestScore = -INF;
    int   alpha     = -INF;
    int   beta      = INF;
    bool  foundLegalMove = false;

    for (const Move& move : moves) {
        // for unmaking the move
        Gamestate prevdata = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare
        };

        // skip illegal moves
        if (!board.makeMove(move)) {
            continue;
        }

        // search at depth - 1
        int score = -minimaxAlphaBeta(board, depth - 1, -beta, -alpha);

        // restore old state exactly as it was before the recursive call
        board.unmakeMove(move, prevdata);

        // if its the first legal move, initialize both bestScore and alpha.
        if (!foundLegalMove || score > bestScore) {
            bestScore      = score;
            bestMove       = move;
            foundLegalMove = true;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    // no move ever passed makeMove() cause all pseudo‐legal moves were actually illegal,
    // return sentinel “noMove,” which UCI will interpret as “0000”.
    if (!foundLegalMove) {
        return Move{NONE, -1, -1 };
    }

    return bestMove;
}

int Search::minimaxAlphaBeta(Board& board, int depth, int alpha, int beta) {
    
    if (depth == 0) {
        return Evaluation::evaluate(board);
    }

    std::vector<Move> moves = board.generateMoves();
    if (moves.empty()) {
        // No pseudo‐legal moves at all → either checkmate or stalemate
        int kingSq = (board.sideToMove == WHITE)
                       ? board.whiteKingSquare
                       : board.blackKingSquare;
        Color opp = (board.sideToMove == WHITE) ? BLACK : WHITE;

        if (board.isSquareAttacked(kingSq, opp)) {
            return -INF + (MAX_DEPTH - depth); // checkmate
        }
        return 0; // stalemate
    }

    bool foundLegalMove = false;

    for (const Move& move : moves) {
        Gamestate prevdata = {
            board.sideToMove,
            board.enPassantSquare,
            board.whiteKingsideCastle,
            board.whiteQueensideCastle,
            board.blackKingsideCastle,
            board.blackQueensideCastle,
            board.whiteKingSquare,
            board.blackKingSquare
        };

        if (!board.makeMove(move)) {
            continue;
        }
        foundLegalMove = true;

        int score = -minimaxAlphaBeta(board, depth - 1, -beta, -alpha);
        board.unmakeMove(move, prevdata);

        if (score >= beta) {
            // Beta cutoff
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    //  there were pseudo‐legal moves but none was actually legal
    if (!foundLegalMove) {
        int kingSq = (board.sideToMove == WHITE)
                       ? board.whiteKingSquare
                       : board.blackKingSquare;
        Color opp = (board.sideToMove == WHITE) ? BLACK : WHITE;

        if (board.isSquareAttacked(kingSq, opp)) {
            // Checkmate
            return -INF + (MAX_DEPTH - depth);
        }
        // Stalemate
        return 0;
    }

    return alpha;
}