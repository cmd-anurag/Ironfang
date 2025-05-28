#include "move.h"
#include "types.h"
#include <string>

std::string moveToAlgrebraic(Move move) {
    if(move.isKingSideCastle) return "O-O";
    if(move.isQueenSideCastle) return "O-O-O";

    std::string result;
    char file_from = 'a' + (move.from % 8);
    int rank_from = 8 - (move.from / 8);
    
    char file_to = 'a' + (move.to % 8);
    int rank_to = 8 - (move.to / 8);

    char p = pieceToChar(move.piece);
    result += p;
    result += file_from;
    result += std::to_string(rank_from);
    result += (move.capture ? 'x' : '-');
    result += file_to;
    result += std::to_string(rank_to);
    
    return result;
}