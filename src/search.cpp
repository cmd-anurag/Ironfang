// search.cpp
#include "search.h"
#include "evaluate.h"
#include <cstdint>
#include <algorithm>
#include <array>

constexpr int INF = 1000000;

std::vector<std::vector<Move>> killerMoves(MAX_DEPTH, std::vector<Move>(2, Move(NONE, -1, -1)));
std::array<std::array<int, 64>, 64> historyHeuristics{};

uint64_t nodeCount = 0;

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

    std::cerr << "Nodes searched: " << nodeCount;
    return bestMove;

}

int Search::minimaxAlphaBeta(Board& board, int depth, int alpha, int beta) {

    ++nodeCount; // an efficiency metric

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

    // Move Ordering
    for(Move &move : moves) {
        int score = 0;

        if(move.capture) {
            score += 1000 + (Evaluation::pieceValue[move.capture] * 10 - Evaluation::pieceValue[move.piece]);
        }

        Color c = getPieceColor(move.piece);
        int targetKingSquare = c == BLACK? board.whiteKingSquare : board.blackKingSquare;
        if(board.tryMove(move) && board.isSquareAttacked(targetKingSquare, c)) {
            score += 500;
        }

        if(move == killerMoves[depth][0]) score += 800;
        if(move == killerMoves[depth][1]) score += 700;

        score += historyHeuristics[move.from][move.to];
        // so much dynamic programming here oof

        move.heuristicScore = score;
    }

    // sort it in descending
    std::sort(moves.begin(), moves.end(), [](const Move &m1, const Move &m2) {
        return m1.heuristicScore > m2.heuristicScore;
    });

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

        // Beta cutoff
        if (score >= beta) {
        
            // record KILLER moves here (nice name)
            if(!move.capture && !(move == killerMoves[depth][0])) {
                killerMoves[depth][1] = killerMoves[depth][0];
                killerMoves[depth][0] = move;
            }

            // record in history heuristics
            if(!move.capture) {
                historyHeuristics[move.from][move.to] += depth * depth; // i'll try favoring deeper nodes
            }

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