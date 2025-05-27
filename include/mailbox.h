#pragma once

namespace MailBox {
    extern const int mailbox[120];
    extern const int mailbox64[64]; // convert a 64-board index to mailbox 120-board index

    inline bool isOnboard(int currentSquare, int offset) {
        return mailbox[mailbox64[currentSquare] + offset] != -1;
    }
}