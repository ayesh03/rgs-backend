#pragma once

#include "lvk_fault_packet.h"
#include <QtGlobal>

/*
 * Low-level Annexure-G Fault Packet Parser (0x19)
 * BIN → LVKFaultPacket
 */

class LVKFaultParser {
public:
    static LVKFaultPacket parse(const quint8* raw, int length);

private:
    // ---- byte readers ----
    static quint8  readU8 (const quint8* d, int& i, int len);
    static quint16 readU16(const quint8* d, int& i, int len);
    static quint32 readU24(const quint8* d, int& i, int len);

    // ---- helpers ----
    static QDateTime readDateTime(const quint8* d, int& i, int len, bool& ok);
};
