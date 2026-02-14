#pragma once

#include <QtGlobal>
#include <QDateTime>
#include <QString>
#include <QVector>

/*
 * Annexure-G (RDSO/SPN/196/2020 Ver 4.0)
 * Packet Type: 0x19 – KAVACH Fault Message to NMS
 *
 * This file ONLY defines structures.
 * No parsing logic here.
 */

// ======================= ENUMS =======================

// KAVACH subsystem type
enum class KavachSubsystemType : quint8 {
    STATIONARY = 0x11,
    ONBOARD    = 0x22,
    TSRMS      = 0x33,
    UNKNOWN    = 0xFF
};

// Fault code nature
enum class FaultCodeType : quint8 {
    FAULT    = 1,
    RECOVERY = 2
};

// ======================= FAULT ITEM =======================

struct LVKFaultItem {
    quint8  moduleId = 0;          // Firm specific
    FaultCodeType codeType;        // Fault / Recovery
    quint16 faultCode = 0;         // Firm specific
};

// ======================= MAIN PACKET =======================

struct LVKFaultPacket {

    // ---- Header ----
    quint16 startOfFrame = 0;       // 0xAAAA (E1) or 0xBBBB (GPRS)
    quint8  messageType = 0x19;
    quint16 messageLength = 0;

    quint16 messageSequence = 0;

    // ---- Identity ----
    quint32 kavachSubsystemId = 0;  // 3 bytes (hex)
    quint16 nmsSystemId = 0;
    quint8  systemVersion = 0;

    // ---- Time ----
    QDateTime packetDateTime;
    bool isDateTimeValid = false;

    // ---- Fault meta ----
    KavachSubsystemType subsystemType = KavachSubsystemType::UNKNOWN;
    quint8 totalFaultCount = 0;

    QVector<LVKFaultItem> faults;

    // ---- Helpers ----
    inline bool isOnboardFault() const {
        return subsystemType == KavachSubsystemType::ONBOARD;
    }

    inline bool isStationaryFault() const {
        return subsystemType == KavachSubsystemType::STATIONARY;
    }
};
