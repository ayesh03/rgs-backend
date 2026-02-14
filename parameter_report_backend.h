#ifndef PARAMETER_REPORT_BACKEND_H
#define PARAMETER_REPORT_BACKEND_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMap>

class ParameterReportBackend
{
public:
    static QJsonObject getReport(
        const QString &fromDate,
        const QString &toDate,
        const QString &logDir
        );

private:
    // Helpers
    static QString hexToBin(const QString &hex);
    static QStringList readBinFile(const QString &filePath);

    // Core
    static void processPacket(
        const QString &station,
        const QString &payloadBin,
        const QString &packetType,
        QMap<QString, QMap<QString, QJsonObject>> &stationParamMap,
        QMap<QString, int> &packetCountMap
        );

    static void decodeEventParameters(
        const QString &station,
        const QString &payloadBin,
        QMap<QString, QMap<QString, QJsonObject>> &stationParamMap
        );

    static QJsonObject makeRow(
        const QString &station,
        const QString &parameter,
        const QJsonValue &value,
        const QString &status
        );
};

#endif
