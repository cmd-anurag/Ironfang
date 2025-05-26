#ifndef TYPES_H
#define TYPES_H


// Piece % 8 = Base Piece Type (1 = Pawn, 2 = Rook, 3 = Knight, 4 = Bishop, 5 = Queen, 6 = King)
enum Piece {
    NONE,
    WP = 1, WR, WN, WB, WQ, WK,
    BP = 9, BR, BN, BB, BQ, BK 
};

enum Color {
    WHITE,
    BLACK,
    NO_COLOR
};

// [1-6] White Pieces
inline bool isWhite(Piece p) {
    return p >= WP && p <= WK;
}

// [9-14] Black Pieces
inline bool isBlack(Piece p) {
    return p >= BP && p <= BK;
}

inline Color getColor(Piece p) {
    if(isWhite(p)) return WHITE;
    if(isBlack(p)) return BLACK;
    return NO_COLOR;
}

inline char pieceToChar(Piece p) {
    switch(p) {
        case WP: return 'P';
        case WR: return 'R';
        case WN: return 'N';
        case WB: return 'B';
        case WQ: return 'Q';
        case WK: return 'K';
        case BP: return 'p';
        case BR: return 'r';
        case BN: return 'n';
        case BB: return 'b';
        case BQ: return 'q';
        case BK: return 'k';
        default: return '.';
    }
}

#endif