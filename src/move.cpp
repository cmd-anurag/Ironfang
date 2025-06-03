#include "move.h"
#include "board.h"
#include "bitboard.h"
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

Move uciToMove(const std::string& uci, BitBoard &board) {
    std::vector<Move> moves = board.generateMoves();

    int fromFile = uci[0] - 'a';
    int fromRank = 7 - (uci[1] - '1');
    int from     = fromRank * 8 + fromFile;

    int toFile   = uci[2] - 'a';
    int toRank   = 7 - (uci[3] - '1');
    int to       = toRank * 8 + toFile;

    char promoChar = (uci.length() == 5) ? uci[4] : '\0';

    for (const Move& move : moves) {
        if (move.from == from && move.to == to) {
            if (promoChar == '\0' || (
                (promoChar == 'q' && (move.promotion == WQ || move.promotion == BQ)) ||
                (promoChar == 'r' && (move.promotion == WR || move.promotion == BR)) ||
                (promoChar == 'b' && (move.promotion == WB || move.promotion == BB)) ||
                (promoChar == 'n' && (move.promotion == WN || move.promotion == BN)))) {
                return move;
            }
        }
    }

    return Move{NONE, -1, -1, 0, 0};  // shouldn't happen unless intentional 
}


// i was getting tired of manually printing move information
std::ostream& operator<<(std::ostream& os, const Move &move) {
    os << "Move Details:\n";
    os << "Piece Moved: " << move.piece << "\n";
    os << "From: " << move.from << "\n";
    os << "To: " << move.to << "\n";
    os << "Captured Piece: " << move.capture << "\n";
    os << "Promoted Piece: " << move.promotion << "\n";
    os << "Is En Passant: " << move.isEnPassant << "\n";
    os << "Is KCastling: " << move.isKingSideCastle << "\n";
    os << "Is QCastling: " << move.isQueenSideCastle << "\n";
    return os;
}

bool operator==(const Move &lhs, const Move &rhs)
{
    return 
    lhs.piece               == rhs.piece && 
    lhs.from                == rhs.from && 
    lhs.to                  == rhs.to &&
    lhs.promotion           == rhs.promotion &&
    lhs.capture             == rhs.capture &&
    lhs.isKingSideCastle    == rhs.isKingSideCastle &&
    lhs.isQueenSideCastle   == rhs.isQueenSideCastle &&
    lhs.isEnPassant         == rhs.isEnPassant;
}

// Add this method to your Move class
bool operator!=(const Move &lhs, const Move &rhs) {
    return !(lhs == rhs);
}