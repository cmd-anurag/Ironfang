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

std::vector<std::vector<Move>> killerMoves(MAX_DEPTH+1, std::vector<Move>(2, Move(NONE, -1, -1)));
std::array<std::array<int, 64>, 64> historyHeuristics{};
TranspositionTable TT;

uint64_t nodeCount = 0;

Move Search::findBestMove(BitBoard& board, int maxDepth, int timeLimit) {
    // Reset counters
    nodeCount = 0;
    TT.lookupCount = 0;
    TT.hitCount = 0;
    TT.keyMatchCount = 0;
    TT.totalStoreAttempts = 0;
    TT.actualStores = 0;
    TT.overwritten = 0;
    TT.currentAge++;
    
    // Start timing
    auto startTime = std::chrono::steady_clock::now();
    int timeForThinking = timeLimit;
    
    // Set a hard cutoff time slightly below the time limit (95%)
    int hardTimeLimit = timeLimit > 0 ? static_cast<int>(timeLimit * 0.95) : -1;
    
    // Default move (will be overwritten soon)
    Move bestMove{NONE, -1, -1};
    int bestScore = -INF;
    
    // Generate all  moves
    std::vector<Move> moves = board.generateMoves();
    
    if (moves.empty()) {
        return Move{NONE, -1, -1}; // No moves available
    }
    
    // Set a default move immediately for safety
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
        
        if (board.makeMove(move)) {
            board.unmakeMove(move, prevdata);
            bestMove = move; // Set default move in case we run out of time
            break;
        }
    }
    
    // ------------------------------------------------------------------------------------
    //                          ITERATIVE DEEPENING SEARCH
    // ------------------------------------------------------------------------------------
    for (int depth = 1; depth <= maxDepth; depth++) {

        

        // Skip time check on depth 1, always do at least one ply
        if (depth > 1 && timeForThinking > 0) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - startTime).count();
                
            // Check if we have enough time for another iteration
            // Use progressively more conservative estimates as we go deeper
            int timeNeededFactor = (depth >= 6) ? 4 : (depth >= 4) ? 3 : 2;
            
            // If the elapsed time * factor > time limit, stop searching
            if (elapsedMs * timeNeededFactor > timeForThinking) {
                break;
            }
        }
        
        // Start a new iteration
        int alpha = -INF;
        int beta = INF;
        int iterationBestScore = -INF;
        Move iterationBestMove = bestMove; // Start with previous best
        
        // Move ordering: start with previous best move
        // std::vector<Move> orderedMoves = moves;
        // auto it = std::find(orderedMoves.begin(), orderedMoves.end(), bestMove);
        // if (it != orderedMoves.end()) {
        //     std::iter_swap(orderedMoves.begin(), it);
        // }

        // Move Ordering for Root Search
        for(Move &move : moves) {
            move.heuristicScore = 0;
            
            if(move==bestMove) {
                move.heuristicScore += 5000;
            }
            
            // 1. MVV-LVA
            if(move.capture) {
                move.heuristicScore += 1000 + (Evaluation::pieceValue[move.capture & 7] * 10 - Evaluation::pieceValue[move.piece & 7]);
            }

        }
        std::sort(moves.begin(), moves.end(), [](const Move& m1, const Move& m2) {
            return m1.heuristicScore > m2.heuristicScore;
        });

        
        // Search all moves at this depth
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
            board.pathDepth = 0;
            // Normal search with full alpha-beta window
            int score = -minimaxAlphaBeta(board, depth - 1, -beta, -alpha, 0);
            
            board.unmakeMove(move, prevdata);
            
            // Check if this is a better move
            if (score > iterationBestScore) {
                iterationBestScore = score;
                iterationBestMove = move;
                
                if (score > alpha) {
                    alpha = score;
                }
            }
            
            // Time check after each move - use hard time limit
            if (hardTimeLimit > 0) {
                auto currentTime = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - startTime).count();
                    
                if (elapsedMs >= hardTimeLimit) {
                    // If we're out of time, exit without updating bestMove
                    // to use results from the previous completed depth
                    return bestMove;
                }
            }
        }
        
        // Only update bestMove if we completed the iteration
        bestMove = iterationBestMove;
        bestScore = iterationBestScore;

        int hashFull = TT.hashfull();

        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - startTime).count();
        
        uint64_t nps = 0;
        if (elapsedMs > 0) {
            nps = (nodeCount * 1000) / elapsedMs; // nodes per second
        }

        // Output information
        std::cout << "info depth " << depth 
                  << " score cp " << bestScore
                  << " nodes " << nodeCount << " hashfull " << hashFull
                  << " nps " << nps
                  << " pv " << moveToUCI(bestMove) << "\n" << std::flush;

        // TT - stats
        // std::cout << "info string TT: depth=" << depth
        //   << " occ=" << TT.entriesOccupied
        //   << " (" << (100.0 * TT.entriesOccupied / TT_SIZE) << "%)"
        //   << " hits=" << TT.hitCount
        //   << " lookups=" << TT.lookupCount
        //   << " hitrate=" << (100.0 * TT.hitCount / std::max(TT.lookupCount, (uint64_t)1ull)) << "%"
        //   << " Key matches=" << (100.0 * TT.keyMatchCount / std::max(TT.lookupCount, (uint64_t)1ull)) << "%"
        //   << " overwrites=" << TT.overwritten
        //   << " stores=" << TT.actualStores
        //   << std::endl;

          
    }

    return bestMove;
}

int Search::minimaxAlphaBeta(BitBoard& board, int depth, int alpha, int beta, int ply) {
    ++nodeCount; // an efficiency metric

    board.repetitionPath[board.pathDepth++] = board.zobristKey;
    assert(board.pathDepth <= 1024);

    int rep = 0;
    for (int i = 0; i < board.pathDepth; ++i) {
        if (board.repetitionPath[i] == board.zobristKey && ++rep >= 3) {
            --board.pathDepth;
            return 0;
        }
    }

    // before doing anything check T-table
    Move tempMove = Move(NONE, -1, -1);
    int tempEval;

    if(TT.probe(board.zobristKey, depth, alpha, beta, tempEval, tempMove)) {
        --board.pathDepth;
        return tempEval;
    }

    int kingSq = (board.sideToMove == WHITE) ? board.whiteKingSquare : board.blackKingSquare;
    Color opponent = (board.sideToMove == WHITE) ? BLACK : WHITE;
    bool inCheck = board.isSquareAttacked(kingSq, opponent);

    if (depth >= 3 && !inCheck && hasNonPawnMaterial(board, board.sideToMove) &&
    beta < MATE_THRESHOLD && alpha > -MATE_THRESHOLD) {
        // snapshot all mutable state
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

        // apply null move
        board.sideToMove = (prevState.sideToMove == WHITE ? BLACK : WHITE);
        board.zobristKey ^= zobristBlackToMove;
        if (prevState.enPassantSquare != -1) {
            int epFile = prevState.enPassantSquare & 7;
            board.zobristKey ^= zobristEnPassant[epFile];
        }
        board.enPassantSquare = -1;

        // reduced-depth, null-window search
        int R = (depth > 6 ? 3 : 2);
        int nullMoveScore = -minimaxAlphaBeta(
            board,
            depth - 1 - R,
            -beta,
            -beta + 1,
            ply+1
        );

        // restore *everything*
        board.sideToMove           = prevState.sideToMove;
        board.enPassantSquare      = prevState.enPassantSquare;
        board.whiteKingsideCastle  = prevState.whiteKingsideCastle;
        board.whiteQueensideCastle = prevState.whiteQueensideCastle;
        board.blackKingsideCastle  = prevState.blackKingsideCastle;
        board.blackQueensideCastle = prevState.blackQueensideCastle;
        board.whiteKingSquare      = prevState.whiteKingSquare;
        board.blackKingSquare      = prevState.blackKingSquare;
        board.zobristKey           = prevState.zobristKey;

        // prune on fail-high
        if (nullMoveScore >= beta) {
            TT.store(board.zobristKey, depth, beta, TT_LOWER, Move(NONE, -1, -1));
            --board.pathDepth;
            return beta;
        }
    }


    if (depth <= 0) {
        --board.pathDepth;
        return quiescenceSearch(board, alpha, beta, 0, ply);
    }

    

    std::vector<Move> moves = board.generateMoves();
    if (moves.empty()) {
        // No pseudo‐legal moves at all → either checkmate or stalemate
        int kingSq = (board.sideToMove == WHITE)
                       ? board.whiteKingSquare
                       : board.blackKingSquare;
        Color opp = (board.sideToMove == WHITE) ? BLACK : WHITE;

        if (board.isSquareAttacked(kingSq, opp)) {
            // Checkmate: encode ply distance so mate in 1 is better than mate in 2
            --board.pathDepth;
            return -INF + ply;
        }
        --board.pathDepth;
        return 0; // stalemate
    }

    // Move Ordering
    for(Move &move : moves) {

        move.heuristicScore = 0;
        int score = 0;

        if(move.capture) {
            score += 1000 + (Evaluation::pieceValue[move.capture & 7] * 10 - Evaluation::pieceValue[move.piece & 7]);
        }
        if(move == tempMove) {
            score += 15000;
        }

        if(move == killerMoves[depth][0]) score += 10000;
        if(move == killerMoves[depth][1]) score += 9000;


        move.heuristicScore = score;
    }

    // sort it in descending
    std::sort(moves.begin(), moves.end(), [](const Move &m1, const Move &m2) {
        return m1.heuristicScore > m2.heuristicScore;
    });

    bool foundLegalMove = false;

    int originalAlpha = alpha;
    Move bestMove(NONE, -1, -1);


    int moveIndex = 0;
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

        // uint64_t originalKey = board.zobristKey;
        if (!board.makeMove(move)) {
            continue;
        }
        foundLegalMove = true;
        

        int score;
        
        // Late Move Reduction
        bool isReducible = !inCheck && !move.capture &&
                    move != killerMoves[depth][0] &&
                    move != killerMoves[depth][1];

        int reduction = 0;
        if (depth >= 3 && moveIndex > 0 && isReducible) {
            reduction = 1;
            if (depth >= 6 && moveIndex >= 4) reduction++;
        }
        
        if (reduction > 0) {
            // Try reduced depth search first
            score = -minimaxAlphaBeta(board, depth - 1 - reduction, -alpha - 1, -alpha, ply+1);
            
            // If it looks promising, do a full search
            if (score > alpha) {
                score = -minimaxAlphaBeta(board, depth - 1, -beta, -alpha, ply+1);
            }
        } else {
            // Regular search for important moves
            score = -minimaxAlphaBeta(board, depth - 1, -beta, -alpha, ply+1);
        }
        board.unmakeMove(move, prevdata);
        ++moveIndex;
        // assert(board.zobristKey == originalKey);

        // Beta cutoff
        if (score >= beta) {
        
            // record KILLER moves here (nice name)
            if(!move.capture && !(move == killerMoves[depth][0])) {
                killerMoves[depth][1] = killerMoves[depth][0];
                killerMoves[depth][0] = move;
            }


            // record a lower bound entry in TT
            TT.store(board.zobristKey,depth, beta, TT_LOWER, move);

            --board.pathDepth;
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
        ? -INF + ply  // checkmate
        : 0;             // stalemate

        TT.store(board.zobristKey, depth, eval, TT_EXACT, Move(NONE, -1, -1));
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

    TT.store(board.zobristKey, depth, alpha, flag, bestMove);
    --board.pathDepth;
    return alpha;
}

int Search::quiescenceSearch(BitBoard& board, int alpha, int beta, int qdepth, int ply) {
    ++nodeCount;

    Move probeMove;
    int probeEval;
    if (TT.probe(board.zobristKey, -qdepth, alpha, beta, probeEval, probeMove)) {
        return probeEval;
    }
    
    // 1. More aggressive depth limit
    if (qdepth >= 4) { 
        int eval = Evaluation::evaluate(board);
        TT.store(board.zobristKey, -qdepth, eval, TT_EXACT, Move(NONE, -1, -1));
        return eval;
    }
    
    // 2. Stand pat evaluation
    int standPat = Evaluation::evaluate(board);
    
    // Stand pat cutoff
    if (standPat >= beta) {
        // Store lower bound in TT
        TT.store(board.zobristKey, -qdepth, beta, TT_LOWER, Move(NONE, -1, -1));
        return beta;
    }
    
    // Update alpha if stand pat is better
    if (alpha < standPat)
        alpha = standPat;
    
    // Generate capture moves
    std::vector<Move> captures = board.generateCaptures();


    for(Move &move : captures) {
        if(move == probeMove) {
            move.heuristicScore += 15000;
        }
        move.heuristicScore += 1000 + Evaluation::pieceValue[move.capture & 7] * 10 - Evaluation::pieceValue[move.piece & 7];
    }
    
    // 4. delta pruning
    const int FUTILITY_MARGIN = 200;
    
    // Order captures by MVV-LVA
    std::sort(captures.begin(), captures.end(), [](const Move& a, const Move& b) {
        return a.heuristicScore > b.heuristicScore;
    });
    
    Move bestMove(NONE, -1, -1);
    
    for (const Move& move : captures) {
        // Delta pruning - skip captures that can't improve alpha
        if (standPat + Evaluation::pieceValue[move.capture&7] + FUTILITY_MARGIN <= alpha) {
            continue;
        }
        
        // Skip obviously bad captures (losing material)
        if (Evaluation::pieceValue[move.capture&7] < Evaluation::pieceValue[move.piece & 7] - 100) {
            continue;
        }
        
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
        
        if (!board.makeMove(move))
            continue;
            
        int score = -quiescenceSearch(board, -beta, -alpha, qdepth + 1, ply+1);
        board.unmakeMove(move, prevdata);
        
        if (score >= beta) {
            // Store lower bound in TT
            TT.store(board.zobristKey, -qdepth, beta, TT_LOWER, move);
            return beta;
        }
            
        if (score > alpha) {
            alpha = score;
            bestMove = move;
        }
    }
    
    TT_FLAG finalFlag = (alpha > standPat) ? TT_EXACT : TT_UPPER;
    TT.store(board.zobristKey, -qdepth, alpha, finalFlag, bestMove);

    return alpha;
}

// Helper function to detect positions where null-move is unsafe
bool Search::hasNonPawnMaterial(const BitBoard& board, Color side) {
    if (side == WHITE) {
        return board.getWhiteKnights() || board.getWhiteBishops() || 
               board.getWhiteRooks() || board.getWhiteQueens();
    } else {
        return board.getBlackKnights() || board.getBlackBishops() || 
               board.getBlackRooks() || board.getBlackQueens();
    }
}