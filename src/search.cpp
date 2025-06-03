// search.cpp
#include "search.h"
#include "evaluate.h"
#include "zobrist.h"
#include "tt.h"
#include <cstdint>
#include <algorithm>
#include <array>
#include <chrono>


bool timeUP = false;
std::chrono::steady_clock::time_point startTime;
constexpr int INF = 1000000;

std::vector<std::vector<Move>> killerMoves(MAX_DEPTH, std::vector<Move>(2, Move(NONE, -1, -1)));
std::array<std::array<int, 64>, 64> historyHeuristics{};
TranspositionTable TT;

uint64_t nodeCount = 0;

Move Search::findBestMove(BitBoard& board, int maxDepth, int timeLimit) {
    Move bestMove{NONE, -1, -1};
    nodeCount = 0;
    TT.hitCount = 0;
    TT.lookupCount = 0;
    timeUP = false;

    // startTime = std::chrono::steady_clock::now();

    // std::vector<Move> initialMoves = board.generateMoves();
    // for (const Move& move : initialMoves) {
    //     // Find the first legal move as a fallback
    //     Gamestate prevdata = {
    //         board.sideToMove, board.enPassantSquare, board.whiteKingsideCastle,
    //         board.whiteQueensideCastle, board.blackKingsideCastle, board.blackQueensideCastle,
    //         board.whiteKingSquare, board.blackKingSquare, board.zobristKey
    //     };
        
    //     if (board.makeMove(move)) {
    //         board.unmakeMove(move, prevdata);
    //         bestMove = move; // Store the first legal move as fallback
    //         break;
    //     }
    // }

    // Iterative Deepening Search from depth 1 to maxDepth
    for (int currentDepth = 1; currentDepth <= maxDepth; currentDepth++) {
        int alpha = -INF;
        int beta = INF;
        int bestScore = -INF;
        bool foundLegalMove = false;
        
        std::vector<Move> moves = board.generateMoves();
        
        if (moves.empty()) {
            return Move{NONE, -1, -1};
        }
        
        // If engine gets a best move from previous iteration, search it first
        if (!(bestMove == Move{NONE, -1, -1})) {
            auto it = std::find(moves.begin(), moves.end(), bestMove);
            if (it != moves.end()) {
                std::swap(*it, moves.front());
            }
        }

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
                board.zobristKey
            };

            if (!board.makeMove(move)) {
                continue;
            }

            int score = -minimaxAlphaBeta(board, currentDepth - 1, -beta, -alpha);
            board.unmakeMove(move, prevdata);

            // if (timeLimit > 0) {
            //     auto currentTime = std::chrono::steady_clock::now();
            //     auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            //         currentTime - startTime).count();
                    
            //     if (elapsed >= timeLimit) {
            //         timeUP = true;
            //         break;
            //     }
            // }

            if (!foundLegalMove || score > bestScore) {
                bestScore = score;
                bestMove = move;
                foundLegalMove = true;
            }
            
            if (score > alpha) {
                alpha = score;
            }
        }

        if (!foundLegalMove) {
            return Move{NONE, -1, -1};
        }

        // if(timeUP) break;
        
        // Display info after each depth iteration
        // double hitRate = (TT.lookupCount == 0) ? 0.0 : 
        //     (100.0 * double(TT.hitCount) / double(TT.lookupCount));
            
        // std::cerr << "Depth " << currentDepth 
        //           << " score: " << bestScore 
        //           << " nodes: " << nodeCount
        //           << " hit rate: " << hitRate << "%" << std::endl << std::endl;
    }

    // Final statistics
    // std::cerr << "Nodes searched: " << nodeCount << '\n';
    // double hitRate = (TT.lookupCount == 0) ? 0.0 : 
    //               (100.0 * double(TT.hitCount) / double(TT.lookupCount));
    // std::cerr << "TT lookups: " << TT.lookupCount
    //           << "  hits: " << TT.hitCount
    //           << "  hit rate: " << hitRate << "%" << std::endl;

    // std::cerr << "Total Store Attempts: " << TT.totalStoreAttempts << "\n";
    // std::cerr << "Actual Stores:        " << TT.actualStores << "\n";
    // std::cerr << "Overwritten Entries:  " << TT.overwritten << "\n";
    // std::cerr << "Currently Occupied:   " << TT.countOccupied() << "\n";
    // std::cerr << "TT Utilization:       " << (100.0 * TT.countOccupied() / TT_SIZE) << "%\n";

    return bestMove;
}

int Search::minimaxAlphaBeta(BitBoard& board, int depth, int alpha, int beta) {
    ++nodeCount; // an efficiency metric

    // before doing anything check T-table
    Move tempMove = Move(NONE, -1, -1);
    int tempEval;
    if(TT.probe(board.zobristKey, depth, alpha, beta, tempEval, tempMove)) {
        return tempEval;
    }

    if (depth == 0) {
        int eval = Evaluation::evaluate(board);
        TT.store(board.zobristKey, 0, eval, TT_EXACT, Move(NONE, -1, -1));
        return eval;
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
        
        // if(move == tempMove) {
        //     score += 0;
        // }

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

    int originalAlpha = alpha;
    Move bestMove(NONE, -1, -1);

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
            board.zobristKey
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

            // record a lower bound entry in TT
            TT.store(board.zobristKey,depth, beta, TT_LOWER, move);

            return beta;
        }

        if (score > alpha) {
            alpha = score;
            bestMove = move;
        }
    }

    //  there were pseudo‐legal moves but none was actually legal
    if (!foundLegalMove) {
        int kingSq = (board.sideToMove == WHITE)
                       ? board.whiteKingSquare
                       : board.blackKingSquare;
        Color opp = (board.sideToMove == WHITE) ? BLACK : WHITE;

        int eval = board.isSquareAttacked(kingSq, opp)
        ? -INF + (MAX_DEPTH - depth)  // checkmate
        : 0;                           // stalemate

        TT.store(board.zobristKey, depth, eval, TT_EXACT, Move(NONE, -1, -1));

        return eval;
    }

    TT_FLAG flag;
    if(alpha <= originalAlpha) {
        flag = TT_UPPER;
    }
    else {
        flag = TT_EXACT;
    }

    TT.store(board.zobristKey, depth, alpha, flag, bestMove);

    return alpha;
}