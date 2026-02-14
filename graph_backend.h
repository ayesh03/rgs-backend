#ifndef GRAPH_BACKEND_H
#define GRAPH_BACKEND_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

class GraphBackend
{
public:
    static QJsonObject initTables();

    static QJsonObject getGraphMeta(
        const QString &logDir,
        const QString &fromDate,
        const QString &toDate
        );


    static QJsonObject getGraphData(
        const QString &locoId,
        const QString &fromDate,
        const QString &toDate,
        const QString &direction,
        const QString &profileId,
        const QString &graphType,
        const QString &logDir
        );


private:
    // helpers
    static QStringList readBinFile(const QString &filePath);
    static bool decodeLocoPacket(
        const QByteArray &pkt,
        quint32 &locoId,
        quint32 &absLoc,
        quint16 &speed,
        quint8  &mode,
        quint8  &direction,
        quint32 &frameNo
        );
};

#endif // GRAPH_BACKEND_H
