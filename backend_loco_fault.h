#pragma once

#include <QJsonObject>
#include <QString>

class BackendLocoFault {
public:
    static QJsonObject fetchByDateRange(
        const QString& fromDate,
        const QString& toDate,
        const QString& logDir
        );

private:
    static bool fileDateInRange(
        const QString& filePath,
        const QDate& from,
        const QDate& to
        );

    static QDate parseFileDate(const QString& fileName);
};
