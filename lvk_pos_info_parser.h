#pragma once

#include "lvk_pos_info_packet.h"
#include <QByteArray>
#include <QString>

class LVKPosInfoParser
{
public:
    static LVKPosInfoPacket parse(const quint8* data, int length);

private:
    static quint8  readU8 (const quint8* d, int& i, int len);
    static quint16 readU16(const quint8* d, int& i, int len);
    static quint32 readU32(const quint8* d, int& i, int len);

    static QString bytesToBitString(const QByteArray& data);
    static quint32 readBits(const QString& bits, int& pos, int length);

    static void parseRegularPacket(const QByteArray& d, int& i, OnboardRegularPacketModel& m);
    static void parseAccessRequest(const QByteArray& d, int& i, OnboardAccessRequestPacketModel& m);
};
