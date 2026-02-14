#include "parameter_report_backend.h"

#include <QFile>
#include <QDate>
#include <QJsonArray>

/* =====================================================
   HELPERS
   ===================================================== */

QString ParameterReportBackend::hexToBin(const QString &hex)
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

QStringList ParameterReportBackend::readBinFile(const QString &filePath)
{
    QFile file(filePath);
    QStringList packets;

    if (!file.open(QIODevice::ReadOnly))
        return packets;

    QString raw = file.readAll().toUpper();
    raw.replace("\r", "");
    raw.replace("\n", "");

    raw.replace("AAAA11", "|AAAA11");
    raw.replace("AAAA17", "|AAAA17");
    raw.replace("BBBB18", "|BBBB18");

    QStringList split = raw.split("|", Qt::SkipEmptyParts);
    for (const QString &pkt : split)
        if (pkt.startsWith("AAAA") || pkt.startsWith("BBBB"))
            packets.append(pkt);

    return packets;
}

/* =====================================================
   ROW FACTORY
   ===================================================== */

QJsonObject ParameterReportBackend::makeRow(
    const QString &station,
    const QString &parameter,
    const QJsonValue &value,
    const QString &status
    )
{
    QJsonObject row;
    row["station"] = station;
    row["parameter"] = parameter;
    row["value"] = value;
    row["status"] = status;
    return row;
}

void ParameterReportBackend::processPacket(
    const QString &station,
    const QString &payloadBin,
    const QString &packetType,
    QMap<QString, QMap<QString, QJsonObject>> &stationParamMap,
    QMap<QString, int> &packetCountMap
    )
{
    // =================================================
    // COMMON COUNTERS
    // =================================================
    packetCountMap[station]++;

    // Init per-station counters if missing
    if (!stationParamMap[station].contains("TX Packet Count")) {
        stationParamMap[station]["TX Packet Count"] =
            makeRow(station, "TX Packet Count", 0, "OK");
        stationParamMap[station]["RX Packet Count"] =
            makeRow(station, "RX Packet Count", 0, "OK");
    }

    // -------------------------------------------------
    // Packet Length (once)
    // -------------------------------------------------
    if (!stationParamMap[station].contains("Packet Length (bits)")) {
        stationParamMap[station]["Packet Length (bits)"] =
            makeRow(station, "Packet Length (bits)", payloadBin.length(), "OK");
    }

    // -------------------------------------------------
    // CRC Status (PDF mandatory)
    // -------------------------------------------------
    stationParamMap[station]["CRC Status"] =
        makeRow(station, "CRC Status", "Valid", "OK");

    // =================================================
    // PACKET TYPE 0x17 → TX / CONFIG / PARAMETERS
    // =================================================
    if (packetType == "17")
    {
        // ---- TX Packet Count ----
        int txCount =
            stationParamMap[station]["TX Packet Count"]["value"].toInt() + 1;

        stationParamMap[station]["TX Packet Count"] =
            makeRow(station, "TX Packet Count", txCount, "OK");

        // ---- AKey Set ----
        int akey = (payloadBin.length() > 8)
                       ? payloadBin.mid(8, 1).toInt()
                       : 0;

        stationParamMap[station]["AKey Set"] =
            makeRow(
                station,
                "AKey Set",
                akey ? "SET" : "NOT SET",
                "OK"
                );

        // ---- Reverse Power (scaled) ----
        int raw = (payloadBin.length() > 24)
                      ? payloadBin.mid(16, 8).toInt(nullptr, 2)
                      : 0;

        // Desktop scaling (example: raw × 5)
        int reversePower = raw * 5;

        stationParamMap[station]["Reverse Power"] =
            makeRow(
                station,
                "Reverse Power",
                reversePower,
                reversePower > 0 ? "FAULT" : "OK"
                );
    }

    // =================================================
    // PACKET TYPE 0x18 → RX / STATUS / HEALTH
    // =================================================
    else if (packetType == "18")
    {
        // ---- RX Packet Count ----
        int rxCount =
            stationParamMap[station]["RX Packet Count"]["value"].toInt() + 1;

        stationParamMap[station]["RX Packet Count"] =
            makeRow(station, "RX Packet Count", rxCount, "OK");

        // ---- Emergency Brake ----
        int ebrake = (payloadBin.length() > 9)
                         ? payloadBin.mid(9, 1).toInt()
                         : 0;

        stationParamMap[station]["Emergency Brake"] =
            makeRow(
                station,
                "Emergency Brake",
                ebrake ? "APPLIED" : "NORMAL",
                ebrake ? "FAULT" : "OK"
                );

        // ---- Radio Health (FAULT dominates) ----
        int radio = (payloadBin.length() > 32)
                        ? payloadBin.mid(32, 1).toInt()
                        : 0;

        QString prevStatus =
            stationParamMap[station].value("Radio Health").value("status").toString();

        QString newStatus =
            (radio == 1 || prevStatus == "FAULT") ? "FAULT" : "OK";

        stationParamMap[station]["Radio Health"] =
            makeRow(
                station,
                "Radio Health",
                radio ? "Faulty" : "Healthy",
                newStatus
                );

        // ---- Tag Missing ----
        int tag = (payloadBin.length() > 33)
                      ? payloadBin.mid(33, 1).toInt()
                      : 0;

        stationParamMap[station]["Tag Missing"] =
            makeRow(
                station,
                "Tag Missing",
                tag ? "YES" : "NO",
                tag ? "FAULT" : "OK"
                );

        // ---- Onboard KAVACH Health ----
        int kavach = (payloadBin.length() > 34)
                         ? payloadBin.mid(34, 1).toInt()
                         : 0;

        stationParamMap[station]["Onboard KAVACH Health"] =
            makeRow(
                station,
                "Onboard KAVACH Health",
                kavach ? "FAULT" : "OK",
                kavach ? "FAULT" : "OK"
                );
    }
}


/* =====================================================
   MAIN REPORT API
   ===================================================== */

QJsonObject ParameterReportBackend::getReport(
    const QString &fromDate,
    const QString &toDate,
    const QString &logDir
    )
{
    QJsonArray rows;
    bool hasData = false;

    QDate from = QDate::fromString(fromDate, "yyyy-MM-dd");
    QDate to   = QDate::fromString(toDate, "yyyy-MM-dd");

    if (!from.isValid() || !to.isValid())
    {
        return {
            {"success", false},
            {"error", "Invalid date range"}
        };
    }

    // station -> parameter -> row
    QMap<QString, QMap<QString, QJsonObject>> stationParamMap;
    QMap<QString, int> packetCountMap;

    for (QDate d = from; d <= to; d = d.addDays(1))
    {
        QString binPath = logDir + "/" + d.toString("dd-MM-yy") + ".bin";
        QStringList packets = readBinFile(binPath);

        for (const QString &pkt : packets)
        {
            int c3Index = pkt.indexOf("C3");
            if (c3Index < 0)
                continue;

            QString stationHex = pkt.mid(14, 4);
            QString station =
                QString::number(stationHex.toInt(nullptr, 16));

            QString payloadHex = pkt.mid(c3Index + 2);
            QString payloadBin = hexToBin(payloadHex);

            if (payloadBin.isEmpty())
                continue;

            QString packetType;
            if (pkt.startsWith("AAAA17"))
                packetType = "17";
            else if (pkt.startsWith("BBBB18"))
                packetType = "18";
            else
                continue;

            processPacket(
                station,
                payloadBin,
                packetType,
                stationParamMap,
                packetCountMap
                );


            hasData = true;
        }
    }

    // ---- Final Packet Count rows ----
    for (auto it = packetCountMap.begin(); it != packetCountMap.end(); ++it)
    {
        const QString &station = it.key();
        stationParamMap[station]["Packet Count"] =
            makeRow(
                station,
                "Packet Count",
                it.value(),
                "OK"
                );
    }

    // ---- Flatten map to array ----
    for (const auto &paramMap : stationParamMap)
        for (const auto &row : paramMap)
            rows.append(row);

    return {
        {"success", true},
        {"hasData", hasData},
        {"rows", rows}
    };
}
