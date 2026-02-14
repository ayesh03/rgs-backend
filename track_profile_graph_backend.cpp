#include "track_profile_graph_backend.h"

#include <QFile>
#include <QDate>
#include <QSet>
#include <QJsonArray>
#include <iostream>

#include "track_profile_config.h"

/* =========================================================
   HEX → BIN
   ========================================================= */
QString TrackProfileGraphBackend::hexToBin(const QString &hex)
{
    QString bin;
    for (QChar c : hex)
    {
        bool ok;
        int v = QString(c).toInt(&ok, 16);
        if (ok)
            bin += QString("%1").arg(v, 4, 2, QChar('0'));
    }
    return bin;
}

/* =========================================================
   READ BIN FILE (AAAA11)
   ========================================================= */
QStringList TrackProfileGraphBackend::readBinFile(const QString &filePath)
{
    QFile file(filePath);
    QStringList packets;

    if (!file.open(QIODevice::ReadOnly))
    {

        return packets;
    }

    QString raw = file.readAll().toUpper();
    raw.replace("\r", "");
    raw.replace("\n", "");

    raw.replace("AAAA11", "|AAAA11");
    QStringList split = raw.split("|", Qt::SkipEmptyParts);

    for (const QString &pkt : split)
    {
        if (pkt.startsWith("AAAA11"))
            packets.append(pkt);
    }


    return packets;
}

/* =========================================================
   META API
   ========================================================= */
QJsonObject TrackProfileGraphBackend::getMeta(
    const QString &logDir,
    const QString &fromDate,
    const QString &toDate
    )
{
    QString cleanLogDir = logDir.trimmed();
    cleanLogDir.replace("\\", "/");

    QSet<QString> locoSet, stationSet, directionSet, profileSet;

    QString fromClean = fromDate.left(10);
    QString toClean   = toDate.left(10);

    QDate from = QDate::fromString(fromClean, "yyyy-MM-dd");
    QDate to   = QDate::fromString(toClean, "yyyy-MM-dd");

    if (!from.isValid() || !to.isValid())
    {

        return {{"success", false}};
    }

    if (!from.isValid() || !to.isValid())
    {

        return {{"success", false}};
    }

    for (QDate d = from; d <= to; d = d.addDays(1))
    {
        QString binPath = cleanLogDir + "/" + d.toString("dd-MM-yy") + ".bin";


        QStringList packets = readBinFile(binPath);

        for (const QString &pkt : packets)
        {
            int a5c3 = pkt.indexOf("A5C3");
            if (a5c3 < 0)
            {

                continue;
            }

            QString payloadHex = pkt.mid(a5c3 + 4);
            QString payloadBin = hexToBin(payloadHex);

            if (!payloadBin.startsWith("1001"))
            {

                continue;
            }

            QString loco =
                QString::number(payloadBin.mid(50, 20).toULongLong(nullptr, 2));
            QString profile =
                QString::number(payloadBin.mid(70, 4).toULongLong(nullptr, 2));

            QString dirBits = payloadBin.mid(99, 2);
            QString direction =
                (dirBits == "01") ? "Nominal" :
                    (dirBits == "10") ? "Reverse" : "NA";

            QString stationHex = pkt.mid(14, 4);
            QString station =
                QString::number(stationHex.toInt(nullptr, 16));

            locoSet.insert(loco);
            // stationSet.insert(station);
            if (STATION_ID_TO_CODE.contains(station))
            {
                stationSet.insert(STATION_ID_TO_CODE.value(station));
            }

            directionSet.insert(direction);
            profileSet.insert(profile);
        }
    }


    return {
        {"success", true},
        {"locos", QJsonArray::fromStringList(locoSet.values())},
        {"stations", QJsonArray::fromStringList(stationSet.values())},
        {"directions", QJsonArray::fromStringList(directionSet.values())},
        {"profiles", QJsonArray::fromStringList(profileSet.values())}
    };
}

/* =========================================================
   GRAPH DATA API
   ========================================================= */
QJsonObject TrackProfileGraphBackend::getGraphData(
    const QString &locoId,
    const QString &station,
    const QString &direction,
    const QString &profileId,
    const QString &fromDate,
    const QString &toDate,
    const QString &logDir
    )
{
    QString cleanLogDir = logDir.trimmed();
    cleanLogDir.replace("\\", "/");

    // if (!STATION_ID_TO_CODE.contains(station))
    // {

    //     return {{"success", false}};
    // }

    QString stationCode = station.trimmed();


    if (!STATION_RANGE_MAP.contains(stationCode))
    {
        return {{"success", false}};
    }



    StationRange r = STATION_RANGE_MAP.value(stationCode);

    int startLoc = (direction == "Nominal") ? r.nominalStart : r.reverseStart;
    int endLoc   = (direction == "Nominal") ? r.nominalEnd   : r.reverseEnd;

    QJsonArray speedGraph, gradientGraph;
    bool hasData = false;

    QString fromClean = fromDate.left(10);
    QString toClean   = toDate.left(10);

    QDate from = QDate::fromString(fromClean, "yyyy-MM-dd");
    QDate to   = QDate::fromString(toClean, "yyyy-MM-dd");



    if (!from.isValid() || !to.isValid())
    {

        return {{"success", false}};
    }

    for (QDate d = from; d <= to; d = d.addDays(1))
    {
        QString binPath = cleanLogDir + "/" + d.toString("dd-MM-yy") + ".bin";



        QStringList packets = readBinFile(binPath);

        for (const QString &pkt : packets)
        {
            int a5c3 = pkt.indexOf("A5C3");
            if (a5c3 < 0) continue;

            QString payloadBin = hexToBin(pkt.mid(a5c3 + 4));
            if (!payloadBin.startsWith("1001")) continue;

            if (QString::number(payloadBin.mid(50,20).toULongLong(nullptr,2)) != locoId)
                continue;

            QString dirBits = payloadBin.mid(99,2);
            QString parsedDir = (dirBits=="01")?"Nominal":(dirBits=="10")?"Reverse":"NA";
            if (parsedDir != direction) continue;

            int pos = 104;

            int sspLen = payloadBin.mid(pos+4,7).toULongLong(nullptr,2);
            sspLen = (sspLen+1)*8;
            QString ssp = payloadBin.mid(pos, sspLen);

            int cnt = ssp.mid(11,5).toULongLong(nullptr,2);
            for(int i=0;i<cnt;i++)
            {
                int dist = ssp.mid(16+21*i,15).toULongLong(nullptr,2);
                int spd  = ssp.mid(32+21*i,6).toULongLong(nullptr,2);

                if (spd >= 1 && spd <= 50)
                {
                    QJsonObject speedPoint;
                    speedPoint["x"] = dist;
                    speedPoint["y"] = spd * 5;
                    speedGraph.append(speedPoint);
                    hasData = true;
                }

            }
            pos += sspLen;

            int gLen = payloadBin.mid(pos+4,7).toULongLong(nullptr,2);
            gLen = (gLen+1)*8;
            QString grad = payloadBin.mid(pos,gLen);
            int gCnt = grad.mid(11,5).toULongLong(nullptr,2);

            for(int i=0;i<gCnt;i++)
            {
                int dist = grad.mid(16+21*i,15).toULongLong(nullptr,2);
                int val  = grad.mid(32+21*i,5).toULongLong(nullptr,2);

                if (val > 0 && val <= 30)
                {
                    QJsonObject gradPoint;
                    gradPoint["x"] = dist;
                    gradPoint["y"] = 1000 / val;
                    gradientGraph.append(gradPoint);
                    hasData = true;
                }

            }

        }
    }
    return {
        {"success", true},
        {"hasData", hasData},
        {"speedGraph", speedGraph},
        {"gradientGraph", gradientGraph}
    };
}
