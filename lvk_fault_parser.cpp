#include "lvk_fault_parser.h"
#include <stdexcept>

// ================= CRC32 (0x04C11DB7) =================
// CRC excludes SOF (first 2 bytes)

static quint32 computeCRC32(const quint8* data, int length)
{
    const quint32 polynomial = 0x04C11DB7;
    quint32 crc = 0xFFFFFFFF;

    for (int i = 0; i < length; ++i) {
        crc ^= (quint32)data[i] << 24;

        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x80000000)
                crc = (crc << 1) ^ polynomial;
            else
                crc <<= 1;
        }
    }

    return crc;
}

// ================= BASIC READERS =================

quint8 LVKFaultParser::readU8(const quint8* d, int& i, int len) {
    if (i >= len) throw std::runtime_error("readU8 OOB");
    return d[i++];
}

quint16 LVKFaultParser::readU16(const quint8* d, int& i, int len) {
    if (i + 1 >= len) throw std::runtime_error("readU16 OOB");
    quint16 v = (d[i] << 8) | d[i + 1];
    i += 2;
    return v;
}

quint32 LVKFaultParser::readU24(const quint8* d, int& i, int len) {
    if (i + 2 >= len) throw std::runtime_error("readU24 OOB");
    quint32 v = (d[i] << 16) | (d[i + 1] << 8) | d[i + 2];
    i += 3;
    return v;
}

// ================= DATE TIME =================

QDateTime LVKFaultParser::readDateTime(
    const quint8* d, int& i, int len, bool& ok)
{
    quint8 day   = readU8(d, i, len);
    quint8 month = readU8(d, i, len);
    quint8 year  = readU8(d, i, len);

    quint8 hh = readU8(d, i, len);
    quint8 mm = readU8(d, i, len);
    quint8 ss = readU8(d, i, len);

    // Annexure allowed ranges validation
    if (day == 0 || day > 31)
        throw std::runtime_error("Invalid day");

    if (month == 0 || month > 12)
        throw std::runtime_error("Invalid month");

    if (hh > 23 || mm > 59 || ss > 59)
        throw std::runtime_error("Invalid time");

    QDateTime dt;
    dt.setDate(QDate(2000 + year, month, day));
    dt.setTime(QTime(hh, mm, ss));

    ok = dt.isValid();
    return dt;
}

// ================= MAIN PARSER =================

LVKFaultPacket LVKFaultParser::parse(const quint8* raw, int length)
{
    if (length < 20)
        throw std::runtime_error("Packet too small");

    LVKFaultPacket pkt;
    int i = 0;

    // ---------------- SOF ----------------
    pkt.startOfFrame = readU16(raw, i, length);

    if (pkt.startOfFrame != 0xAAAA &&
        pkt.startOfFrame != 0xBBBB)
        throw std::runtime_error("Invalid SOF");

    // ---------------- Message Type ----------------
    pkt.messageType = readU8(raw, i, length);

    if (pkt.messageType != 0x19)
        throw std::runtime_error("Invalid Message Type");

    // ---------------- Message Length ----------------
    pkt.messageLength = readU16(raw, i, length);

    // Annexure: Length = from Message Type to CRC inclusive
    if (pkt.messageLength != (length - 2))
        throw std::runtime_error("Invalid Message Length");

    // ---------------- Sequence ----------------
    pkt.messageSequence = readU16(raw, i, length);

    // ---------------- IDs ----------------
    pkt.kavachSubsystemId = readU24(raw, i, length);
    pkt.nmsSystemId       = readU16(raw, i, length);
    pkt.systemVersion     = readU8(raw, i, length);

    // ---------------- Date Time ----------------
    pkt.packetDateTime =
        readDateTime(raw, i, length, pkt.isDateTimeValid);

    if (!pkt.isDateTimeValid)
        throw std::runtime_error("Invalid DateTime");

    // ---------------- Subsystem Type ----------------
    quint8 subsystemRaw = readU8(raw, i, length);

    // Annexure-G valid values:
    // 0x11 – Stationary
    // 0x22 – Onboard
    // 0x33 – TSRMS
    if (subsystemRaw != 0x11 &&
        subsystemRaw != 0x22 &&
        subsystemRaw != 0x33)
    {
        throw std::runtime_error("Invalid Subsystem Type");
    }

    pkt.subsystemType =
        static_cast<KavachSubsystemType>(subsystemRaw);


    // ---------------- Total Fault Count ----------------
    pkt.totalFaultCount = readU8(raw, i, length);

    if (pkt.totalFaultCount > 10)
        throw std::runtime_error("Fault count exceeds Annexure limit");

    // ---------------- Fault List ----------------
    for (int f = 0; f < pkt.totalFaultCount; ++f) {

        LVKFaultItem item;

        item.moduleId = readU8(raw, i, length);

        quint8 type = readU8(raw, i, length);

        if (type != 1 && type != 2)
            throw std::runtime_error("Invalid Fault Code Type");

        item.codeType =
            (type == 1)
                ? FaultCodeType::FAULT
                : FaultCodeType::RECOVERY;

        item.faultCode = readU16(raw, i, length);

        pkt.faults.append(item);
    }

    // ---------------- CRC ----------------
    if (i + 4 > length)
        throw std::runtime_error("CRC missing");

    quint32 receivedCRC =
        (raw[i] << 24) |
        (raw[i + 1] << 16) |
        (raw[i + 2] << 8) |
        raw[i + 3];

    // Compute CRC excluding SOF
    quint32 calculatedCRC =
        computeCRC32(raw + 2, pkt.messageLength - 4);

    if (receivedCRC != calculatedCRC)
        throw std::runtime_error("CRC mismatch");

    return pkt;
}
