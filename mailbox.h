#pragma once

namespace MailBox {
    extern const int mailbox[120];
    extern const int mailbox64[64]; // convert a 64-board index to mailbox 120-board index

    inline bool isOnboard(int square) {
        return square >=0 && square < 64 && mailbox[mailbox64[square]] != -1;
    }
}