#pragma once

#include <QJsonObject>
#include <QString>

class TrackProfileGraphBackend
{
public:
    // helpers
    static QString hexToBin(const QString &hex);
    static QStringList readBinFile(const QString &filePath);

    // APIs
    static QJsonObject getMeta(
        const QString &logDir,
        const QString &fromDate,
        const QString &toDate
        );

    static QJsonObject getGraphData(
        const QString &locoId,
        const QString &station,
        const QString &direction,
        const QString &profileId,
        const QString &fromDate,
        const QString &toDate,
        const QString &logDir
        );
};
