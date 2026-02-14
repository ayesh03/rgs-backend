#include "backend_loco_fault.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <iostream>

#define JNUM(x) QJsonValue(static_cast<qint64>(x))

// =====================================================
// FILE DATE PARSER (DD-MM-YY.bin)
// =====================================================
QDate BackendLocoFault::parseFileDate(const QString& filePath)
{
    QString base = QFileInfo(filePath).baseName(); // dd-MM-yy
    QStringList p = base.split('-');
    if (p.size() != 3)
        return QDate();

    return QDate(2000 + p[2].toInt(),
                 p[1].toInt(),
                 p[0].toInt());
}
static quint32 calculateCRC32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFF;

    for (unsigned char byte : data)
    {
        crc ^= byte;

        for (int i = 0; i < 8; ++i)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFFFFFF;
}

// =====================================================
// MAIN API
// =====================================================
QJsonObject BackendLocoFault::fetchByDateRange(
    const QString& fromDate,
    const QString& toDate,
    const QString& logDir)
{
    QJsonArray rows;

    QString decodedFrom = QUrl::fromPercentEncoding(fromDate.toUtf8()).trimmed();
    QString decodedTo   = QUrl::fromPercentEncoding(toDate.toUtf8()).trimmed();
    QString folder      = QUrl::fromPercentEncoding(logDir.toUtf8()).trimmed();

    QDateTime fromDt = QDateTime::fromString(decodedFrom, Qt::ISODate);
    QDateTime toDt   = QDateTime::fromString(decodedTo, Qt::ISODate);

    if (!fromDt.isValid() || !toDt.isValid())
        return {{"success", false}, {"error", "Invalid date"}};

    if (!QDir(folder).exists())
        return {{"success", false}, {"error", "Invalid logDir"}};

    QStringList files =
        QDir(folder).entryList({"*.bin"}, QDir::Files, QDir::Name);

    // =====================================================
    // FILE LOOP
    // =====================================================
    for (const QString& file : files)
    {
        QString fullPath = folder + "/" + file;

        QDate fileDate = parseFileDate(fullPath);
        if (!fileDate.isValid())
            continue;

        if (fileDate < fromDt.date() || fileDate > toDt.date())
            continue;

        QFile f(fullPath);
        if (!f.open(QIODevice::ReadOnly))
            continue;

        QTextStream in(&f);

        // =====================================================
        // LINE LOOP
        // =====================================================
        while (!in.atEnd())
        {
            QString line = in.readLine().trimmed();

            line = line.toUpper();

            if (!line.contains("AAAA19") &&
                !line.contains("BBBB19"))
                continue;


            bool isStation = line.startsWith("AAAA19");

            QByteArray raw = QByteArray::fromHex(line.toLatin1());

            if (raw.size() < 29)
                continue;

            const quint8* d =
                reinterpret_cast<const quint8*>(raw.constData());

            int idx = 0;

            // ---------------- SOF ----------------
            quint16 sof = (d[idx] << 8) | d[idx + 1];
            idx += 2;

            // ---------------- MSG TYPE ----------------
            quint8 msgType = d[idx++];
            if (msgType != 0x19)
                continue;

            // ---------------- MSG LENGTH ----------------
            quint16 msgLength =
                (d[idx] << 8) | d[idx + 1];
            idx += 2;
            if (msgLength != raw.size() - 2)
                continue;


            // ---------------- SEQ ----------------
            quint16 msgSeq =
                (d[idx] << 8) | d[idx + 1];
            idx += 2;

            // ---------------- KAVACH SUBSYSTEM ID (3 bytes) ----------------
            quint32 kavachId =
                (d[idx] << 16) |
                (d[idx + 1] << 8) |
                d[idx + 2];
            idx += 3;

            // ---------------- NMS SYSTEM ID ----------------
            quint16 nmsId =
                (d[idx] << 8) | d[idx + 1];
            idx += 2;

            // ---------------- VERSION ----------------
            quint8 version = d[idx++];

            // ---------------- DATE ----------------
            quint8 day   = d[idx++];
            quint8 month = d[idx++];
            quint8 year  = d[idx++];


            // ---------------- TIME ----------------
            quint8 hh = d[idx++];
            quint8 mm = d[idx++];
            quint8 ss = d[idx++];

            QDateTime pktTime(
                QDate(2000 + year, month, day),
                QTime(hh, mm, ss)
                );

            // if (!pktTime.isValid())
            //     continue;

            // OPTIONAL time filter (uncomment if needed)
            /*
            if (pktTime < fromDt || pktTime > toDt)
                continue;
            */

            // ---------------- SUBSYSTEM TYPE ----------------
            quint8 subsystemType = d[idx++];

            // ---------------- FAULT COUNT ----------------
            quint8 totalFault = d[idx++];
            if (totalFault > 10)
                continue;
            // ---- CRC VALIDATION ----
            quint32 receivedCrc =
                (d[raw.size()-4] << 24) |
                (d[raw.size()-3] << 16) |
                (d[raw.size()-2] << 8)  |
                d[raw.size()-1];

            // Exclude SOF (first 2 bytes) and CRC (last 4 bytes)
            QByteArray crcData = raw.mid(2, raw.size() - 6);

            quint32 calculatedCrc = calculateCRC32(crcData);

            // if (receivedCrc != calculatedCrc)
            //     continue;



            // =====================================================
            // FAULT LOOP
            // =====================================================
            for (int f = 0; f < totalFault; ++f)
            {
                if (idx + 4 > raw.size())
                    break;

                quint8 moduleId = d[idx++];
                quint8 type     = d[idx++];
                quint16 code    =
                    (d[idx] << 8) | d[idx + 1];
                idx += 2;

                QJsonObject row;

                row["sof"] = isStation ? "AAAA" : "BBBB";
                row["fault_origin"] =
                    isStation ? "STATION" : "LOCO";

                row["event_time"] = pktTime.toString("yyyy-MM-dd HH:mm:ss");



                row["packet_type"] = 0x19;

                row["message_sequence"] = JNUM(msgSeq);
                row["kavach_subsystem_id"] = JNUM(kavachId);
                row["nms_system_id"] = JNUM(nmsId);
                row["system_version"] = JNUM(version);


                QString moduleHex =
                    QString("%1")
                        .arg(moduleId, 2, 16, QChar('0'))
                        .toUpper();

                QString subsystemHex =
                    QString("%1")
                        .arg(subsystemType, 2, 16, QChar('0'))
                        .toUpper();

                row["fault_module_id"] = moduleHex;
                row["subsystem_type"] = subsystemHex;


                QString faultCodeHex =
                    QString("%1")
                        .arg(code, 4, 16, QChar('0'))
                        .toUpper();

                row["fault_code"] = faultCodeHex;


                QString faultTypeHex =
                    QString("%1")
                        .arg(type, 2, 16, QChar('0'))
                        .toUpper();

                row["fault_type"] = faultTypeHex;


                row["data_source"] = "BIN";

                rows.append(row);
            }
        }
    }

    return {
        {"success", true},
        {"data", rows}
    };
}
