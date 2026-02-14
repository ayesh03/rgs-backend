#pragma once

#include <QDateTime>

struct OnboardRegularPacketModel {
    quint32 FrameNumber = 0;
    quint32 SourceLocoId = 0;
    quint8  SourceLocoVersion = 0;
    quint32 AbsoluteLocoLocation = 0;
    quint32 LDoubtOver = 0;
    quint32 LDoubtUnder = 0;
    quint8  TrainIntegrity = 0;
    quint32 TrainLength = 0;
    quint32 TrainSpeed = 0;
    quint8  MovementDir = 0;
    quint8  EmergencyStatus = 0;
    quint8  LocoMode = 0;
    quint32 LastRfidTag = 0;
    quint8  TagDup = 0;
    quint8  TagLinkInfo = 0;
    quint32 Tin = 0;
    quint8  BrakeApplied = 0;
    quint8  NewMaReply = 0;
    quint8  LastRefProfileNum = 0;
    quint8  SignalOverride = 0;
    quint8  InfoAck = 0;
    quint8  OnboardHealthBytes[3]{};
};

struct OnboardAccessRequestPacketModel {
    quint32 PacketType = 0;
    quint32 FrameNumber = 0;
    quint32 SourceLocoId = 0;
    quint8  SourceLocoVersion = 0;
    quint32 AbsoluteLocoLocation = 0;
    quint32 TrainLength = 0;
    quint32 TrainSpeed = 0;
    quint8  MovementDir = 0;
    quint8  EmergencyStatus = 0;
    quint8  LocoMode = 0;
    quint32 ApprochingStationID = 0;
    quint32 LastRFIDTag = 0;
    quint32 Tin = 0;
    quint32 Longitude = 0;
    quint32 Latitude = 0;
    quint8  LOCO_RND_NUM_RL = 0;
};

struct LVKPosInfoPacket {
    int PacketType = 0;
    quint16 MessageSequence = 0;
    quint16 StationaryKavachId = 0;
    quint16 NmsSystemId = 0;
    quint8  SystemVersion = 0;

    QDateTime PacketDateTime;
    bool IsDateTimeValid = false;

    quint8 ActiveRadio = 0;
    quint8 NoOfMASections = 0;
    quint16 RouteId = 0;

    OnboardRegularPacketModel RadioPacket;
    OnboardAccessRequestPacketModel ARRadioPacket;
};
