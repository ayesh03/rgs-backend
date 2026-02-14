#pragma once
#include <QJsonObject>

class BackendRFCOMFault
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
        const QString &toDate
        );

    static QJsonObject insertFault(const QJsonObject &p);
    static QJsonObject getStations();

};
