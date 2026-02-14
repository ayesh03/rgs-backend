#pragma once

#include <QJsonObject>
#include <QString>
#include <QDate>

    class BackendStationaryKavach
{
public:
    // -------- APIs --------
    static QJsonObject fetchRegular(
        const QString& fromDate,
        const QString& toDate,
        const QByteArray& fileData
        );


    static QJsonObject fetchAccess(
        const QString& fromDate,
        const QString& toDate,
        const QByteArray& fileData
        );

    static QJsonObject fetchEmergency(
        const QString& fromDate,
        const QString& toDate,
        const QByteArray& fileData
        );

private:
    // -------- helpers --------
    static QJsonObject processBinFiles(
        const QString& fromDate,
        const QString& toDate,
        const QByteArray& fileData,
        int expectedPktTypeBits
        );


    static bool fileDateInRange(
        const QString& filePath,
        const QDate& from,
        const QDate& to
        );

    static QDate parseFileDate(const QString& fileName);
};

