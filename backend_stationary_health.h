#ifndef BACKEND_STATIONARY_HEALTH_H
#define BACKEND_STATIONARY_HEALTH_H

#include <QJsonObject>
#include <QString>

class BackendStationaryHealth
{
public:
    static QJsonObject fetchStationHealth(
        const QString& fromDate,
        const QString& toDate,
        const QString& logDir);

    static QJsonObject fetchOnboardHealth(
        const QString& fromDate,
        const QString& toDate,
        const QString& logDir);

private:
    static QJsonObject processBinFiles(
        const QString& fromDate,
        const QString& toDate,
        const QString& logDir,
        quint8 expectedMsgType);   // 0x17 or 0x18

    static int getEventDataSize_0x17(quint16 eventId);
    static int getEventDataSize_0x18(quint16 eventId);
};

#endif
