#include "track_profile_report_backend.h"

#include <QFile>
#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include "track_profile_config.h"

QJsonObject TrackProfileReportBackend::getAllStations()
{
    QJsonArray stations;

    // Desktop parity: return ALL configured stations
    for (auto it = STATION_ID_TO_CODE.begin();
         it != STATION_ID_TO_CODE.end(); ++it)
    {
        QJsonObject st;
        st["stationId"]   = it.key();        // "601"
        st["stationCode"] = it.value();      // "ARC_AMSA"
        stations.append(st);
    }

    return {
        {"success", true},
        {"stations", stations}
    };
}

/* =========================================================
   LOW LEVEL HELPERS (DESKTOP + GRAPH PARITY)
   ========================================================= */

static QString hexToBin(const QString &hex)
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

static QStringList extractAAAA11Packets(const QString &filePath)
{
    QFile file(filePath);
    QStringList packets;

    if (!file.open(QIODevice::ReadOnly))
        return packets;

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
   TRACK PROFILE REPORT API (DESKTOP PARITY)
   ========================================================= */

QJsonObject TrackProfileReportBackend::getReport(
    const QString &fromDate,
    const QString &toDate,
    const QString &logDir,
    const QStringList &stations
    )
{
    QJsonArray rows;
    bool hasData = false;

    QString fromClean = fromDate.left(10);
    QString toClean   = toDate.left(10);

    QDate from = QDate::fromString(fromClean, "yyyy-MM-dd");
    QDate to   = QDate::fromString(toClean,   "yyyy-MM-dd");


    if (!from.isValid() || !to.isValid())
    {
        return {
            {"success", false},
            {"error", "Invalid date range"}
        };
    }

    QString cleanLogDir = logDir;
    cleanLogDir.replace("\\", "/");

    for (QDate d = from; d <= to; d = d.addDays(1))
    {
        QString binPath =
            cleanLogDir + "/" + d.toString("dd-MM-yy") + ".bin";

        QStringList packets = extractAAAA11Packets(binPath);

        for (const QString &pkt : packets)
        {
            /* -------------------------
               Must contain A5 C3
               ------------------------- */
            int a5c3 = pkt.indexOf("A5C3");
            if (a5c3 < 0)
                continue;

            QString payloadHex = pkt.mid(a5c3 + 4);
            QString payloadBin = hexToBin(payloadHex);

            /* -------------------------
               Packet Type = 1001 (Track Profile)
               ------------------------- */
            if (!payloadBin.startsWith("1001"))
                continue;

            /* -------------------------
               Station ID (from BIN)
               ------------------------- */
            QString stationHex = pkt.mid(14, 4);
            QString stationId =
                QString::number(stationHex.toInt(nullptr, 16));

            // Optional station filter (desktop behavior)
            if (!stations.isEmpty() &&
                !stations.contains(stationId))
            {
                continue;
            }

            /* -------------------------
               Date (from MAIN packet)
               ------------------------- */
            int day   = pkt.mid(24, 2).toInt(nullptr, 16);
            int month = pkt.mid(26, 2).toInt(nullptr, 16);
            int year  = pkt.mid(28, 2).toInt(nullptr, 16);

            QString date =
                QString("%1-%2-%3")
                    .arg(day,   2, 10, QChar('0'))
                    .arg(month, 2, 10, QChar('0'))
                    .arg(year,  2, 10, QChar('0'));

            /* -------------------------
               Time + Frame Number
               ------------------------- */
            int hh = pkt.mid(30, 2).toInt(nullptr, 16);
            int mm = pkt.mid(32, 2).toInt(nullptr, 16);
            int ss = pkt.mid(34, 2).toInt(nullptr, 16);

            QString time =
                QString("%1:%2:%3")
                    .arg(hh, 2, 10, QChar('0'))
                    .arg(mm, 2, 10, QChar('0'))
                    .arg(ss, 2, 10, QChar('0'));

            int frameNumber =
                (hh * 3600) + (mm * 60) + ss + 1;


            /* -------------------------
               DEST LOCO ID (bits 49–68)
               ------------------------- */
            QString locoId =
                QString::number(
                    payloadBin.mid(49, 20)
                        .toULongLong(nullptr, 2)
                    );

            /* -------------------------
               PROFILE ID (bits 69–72)
               ------------------------- */
            QString profileId =
                QString::number(
                    payloadBin.mid(69, 4)
                        .toULongLong(nullptr, 2)
                    );

            /* -------------------------
               SUB PROFILE COUNT (bits 269–275)
               ------------------------- */
            QString subProfileCount =
                QString::number(
                    payloadBin.mid(269, 7)
                        .toULongLong(nullptr, 2)
                    );
            /* -------------------------
               PROFILE LENGTH (bits 264–270)
               ------------------------- */
            QString profileLength =
                QString::number(
                    payloadBin.mid(264, 7)
                        .toULongLong(nullptr, 2)
                    );

            /* -------------------------
               FINAL ROW (DESKTOP FORMAT)
               ------------------------- */
            QJsonObject row;
            row["date"]              = date;
            row["time"]              = time;
            row["frameNumber"]       = frameNumber;
            row["stationId"]         = stationId;
            row["locoId"]            = locoId;
            row["profileId"]         = profileId;
            row["subProfileCount"]   = subProfileCount;
            row["subProfileId"]      = "-";
            row["startLocation"]     = "-";
            row["profileLength"]     = profileLength;

            rows.append(row);
            hasData = true;
        }
    }

    return {
        {"success", true},
        {"hasData", hasData},
        {"rows", rows}
    };
}
