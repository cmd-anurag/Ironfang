#include "move.h"
#include "types.h"
#include <string>

std::string moveToAlgrebraic(Move move) {
    if(move.isKingSideCastle) return "O-O";
    if(move.isQueenSideCastle) return "O-O-O";

    std::string result;
    char file_from = 'a' + (move.from % 8);
    // int rank_from = 8 - (move.from / 8);
    
    char file_to = 'a' + (move.to % 8);
    int rank_to = 8 - (move.to / 8);

    
    if (move.piece != WP && move.piece != BP) {
        result += pieceToChar(move.piece);
        
        // NOTE: In a complete implementation, i would check here if i need
        // to disambiguate the move by adding file or rank of origin
        // i'll think about it later for now
    }
    
    else if (move.capture) {
        result += file_from;
    }
    
    
    if (move.capture) {
        result += 'x';
    }
    
    
    result += file_to;
    result += std::to_string(rank_to);
    
    
    if (move.promotion) {
        result += '=';
        switch (move.promotion) {
            case WQ: case BQ: result += 'Q'; break;
            case WR: case BR: result += 'R'; break;
            case WB: case BB: result += 'B'; break;
            case WN: case BN: result += 'N'; break;
        }
    }
    
    // NOTE: For a complete implementation, i should add '+' for check
    // and '#' for checkmate, but this requires board state information so i'll skip it for now
    
    return result;
}

std::string moveToUCI(Move move) {
    std::string result;
    
   
    char file_from = 'a' + (move.from % 8);
    int rank_from = 8 - (move.from / 8);
    
    char file_to = 'a' + (move.to % 8);
    int rank_to = 8 - (move.to / 8);
    
    
    result += file_from;
    result += std::to_string(rank_from);
    
    
    result += file_to;
    result += std::to_string(rank_to);
    
   
    if (move.promotion) {
        char promotionChar = ' ';
        switch (move.promotion) {
            case WQ: case BQ: promotionChar = 'q'; break;
            case WR: case BR: promotionChar = 'r'; break;
            case WB: case BB: promotionChar = 'b'; break;
            case WN: case BN: promotionChar = 'n'; break;
        }
        result += promotionChar;
    }
    
    return result;
}