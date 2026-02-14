#include "lvk_pos_info_parser.h"
#include <QtMath>
#include <stdexcept>

LVKPosInfoPacket LVKPosInfoParser::parse(const quint8* raw, int length)
{
    QByteArray data(reinterpret_cast<const char*>(raw), length);

    LVKPosInfoPacket pkt;
    int idx = 2; // AA AA

    auto messageType = readU8(raw, idx, length);

    quint16 msgLen   = readU16(raw, idx, length);
    pkt.MessageSequence = readU16(raw, idx, length);
    pkt.StationaryKavachId = readU16(raw, idx, length);
    pkt.NmsSystemId = readU16(raw, idx, length);

    pkt.SystemVersion = readU8(raw, idx, length);

    quint8 day   = readU8(raw, idx, length);
    quint8 month = readU8(raw, idx, length);
    quint8 year  = readU8(raw, idx, length);

    quint8 hour   = readU8(raw, idx, length);
    quint8 minute = readU8(raw, idx, length);
    quint8 second = readU8(raw, idx, length);

    QDateTime dt;
    dt.setDate(QDate(2000 + year, month, day));
    dt.setTime(QTime(hour, minute, second));

    pkt.PacketDateTime = dt;
    pkt.IsDateTimeValid = dt.isValid();

    pkt.ActiveRadio = readU8(raw, idx, length);

    idx += 2; // sof tx

    quint8 pktTypeLen = raw[idx++];
    pkt.PacketType = (pktTypeLen >> 4) & 0x0F;

    if (pkt.PacketType == 0xA) {
        parseRegularPacket(data, idx, pkt.RadioPacket);
    } else if (pkt.PacketType == 0xD) {
        parseAccessRequest(data, idx, pkt.ARRadioPacket);
    }

    idx = length - 7;
    pkt.NoOfMASections = readU8(raw, idx, length);
    pkt.RouteId = readU16(raw, idx, length);

    return pkt;
}


// ================= BASIC READERS =================

quint8 LVKPosInfoParser::readU8(const quint8* d, int& i, int len)
{
    if (i >= len)
        throw std::runtime_error("readU8 out of bounds");
    return d[i++];
}

quint16 LVKPosInfoParser::readU16(const quint8* d, int& i, int len)
{
    if (i + 1 >= len)
        throw std::runtime_error("readU16 out of bounds");

    quint16 v = (d[i] << 8) | d[i + 1];
    i += 2;
    return v;
}

quint32 LVKPosInfoParser::readU32(const quint8* d, int& i, int len)
{
    if (i + 3 >= len)
        throw std::runtime_error("readU32 out of bounds");

    quint32 v =
        (quint32(d[i]) << 24) |
        (quint32(d[i + 1]) << 16) |
        (quint32(d[i + 2]) << 8) |
        quint32(d[i + 3]);

    i += 4;
    return v;
}

// ================= BIT HELPERS =================

QString LVKPosInfoParser::bytesToBitString(const QByteArray& data)
{
    QString bits;
    bits.reserve(data.size() * 8);

    for (quint8 b : data)
        bits += QString("%1").arg(b, 8, 2, QChar('0'));

    return bits;
}

quint32 LVKPosInfoParser::readBits(const QString& bits, int& pos, int length)
{
    if (pos + length > bits.length())
        throw std::runtime_error("Bit overflow");

    quint32 value = bits.mid(pos, length).toUInt(nullptr, 2);
    pos += length;
    return value;
}

// ================= REGULAR PACKET =================

void LVKPosInfoParser::parseRegularPacket(
    const QByteArray& d, int& i, OnboardRegularPacketModel& m)
{
    QByteArray payload = d.mid(i - 1);
    QString bits = bytesToBitString(payload);
    int p = 0;

    quint32 pktType = readBits(bits, p, 4);
    quint32 pktLength = readBits(bits, p, 7);

    if (pktType != 0b1010)
        throw std::runtime_error("Not Regular Packet");

    m.FrameNumber = readBits(bits, p, 17);
    m.SourceLocoId = readBits(bits, p, 20);
    m.SourceLocoVersion = readBits(bits, p, 3);
    m.AbsoluteLocoLocation = readBits(bits, p, 23);
    m.LDoubtOver = readBits(bits, p, 9);
    m.LDoubtUnder = readBits(bits, p, 9);
    m.TrainIntegrity = readBits(bits, p, 2);
    m.TrainLength = readBits(bits, p, 11);
    m.TrainSpeed = readBits(bits, p, 9);
    m.MovementDir = readBits(bits, p, 2);
    m.EmergencyStatus = readBits(bits, p, 3);
    m.LocoMode = readBits(bits, p, 4);
    m.LastRfidTag = readBits(bits, p, 10);
    m.TagDup = readBits(bits, p, 1);
    m.TagLinkInfo = readBits(bits, p, 3);
    m.Tin = readBits(bits, p, 9);
    m.BrakeApplied = readBits(bits, p, 3);
    m.NewMaReply = readBits(bits, p, 2);
    m.LastRefProfileNum = readBits(bits, p, 4);
    m.SignalOverride = readBits(bits, p, 1);
    m.InfoAck = readBits(bits, p, 4);

    p += 2; // spare

    quint32 health = readBits(bits, p, 24);
    m.OnboardHealthBytes[0] = health >> 16;
    m.OnboardHealthBytes[1] = health >> 8;
    m.OnboardHealthBytes[2] = health;

    p += 64; // MAC + CRC
}

// ================= ACCESS REQUEST =================

void LVKPosInfoParser::parseAccessRequest(
    const QByteArray& d, int& i, OnboardAccessRequestPacketModel& m)
{
    QByteArray payload = d.mid(i - 1);
    QString bits = bytesToBitString(payload);
    int p = 0;

    quint32 pktType = readBits(bits, p, 4);
    readBits(bits, p, 7); // length

    if (pktType != 0b1101)
        throw std::runtime_error("Not Access Request Packet");

    m.PacketType = pktType;
    m.FrameNumber = readBits(bits, p, 17);
    m.SourceLocoId = readBits(bits, p, 20);
    m.SourceLocoVersion = readBits(bits, p, 3);
    m.AbsoluteLocoLocation = readBits(bits, p, 23);
    m.TrainLength = readBits(bits, p, 11);
    m.TrainSpeed = readBits(bits, p, 9);
    m.MovementDir = readBits(bits, p, 2);
    m.EmergencyStatus = readBits(bits, p, 3);
    m.LocoMode = readBits(bits, p, 4);
    m.ApprochingStationID = readBits(bits, p, 16);
    m.LastRFIDTag = readBits(bits, p, 10);
    m.Tin = readBits(bits, p, 9);
    m.Longitude = readBits(bits, p, 21);
    m.Latitude = readBits(bits, p, 20);
    m.LOCO_RND_NUM_RL = readBits(bits, p, 4);
}
