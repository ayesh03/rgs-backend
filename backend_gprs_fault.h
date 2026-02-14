// backend_gprs_fault.h
#pragma once

#include <QJsonObject>

class BackendGPRSFault
{
public:
    static QJsonObject initTable();
    static QJsonObject getFaults(
        const QString &station,
        int page,
        int limit
        );
    static QJsonObject getFaultsByDateRange(
        const QString &station,
        const QString &fromDate,
        const QString &toDate,
        int page,
        int limit
        );
    static QJsonObject insertFault(const QJsonObject &payload);
    static QJsonObject getStations();

};
