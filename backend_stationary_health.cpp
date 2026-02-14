#include "backend_stationary_health.h"

#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QJsonArray>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>

#define JNUM(x) QJsonValue(static_cast<qint64>(x))

// =====================================================
// PUBLIC API
// =====================================================

QJsonObject BackendStationaryHealth::fetchStationHealth(
    const QString& fromDate,
    const QString& toDate,
    const QString& logDir)
{
    return processBinFiles(fromDate, toDate, logDir, 0x17);
}

QJsonObject BackendStationaryHealth::fetchOnboardHealth(
    const QString& fromDate,
    const QString& toDate,
    const QString& logDir)
{
    return processBinFiles(fromDate, toDate, logDir, 0x18);
}

// =====================================================
// CORE PROCESSOR
// =====================================================

QJsonObject BackendStationaryHealth::processBinFiles(
    const QString& fromDate,
    const QString& toDate,
    const QString& logDir,
    quint8 expectedMsgType)
{
    QJsonArray rows;

    QDate fromD = QDateTime::fromString(
                      QUrl::fromPercentEncoding(fromDate.toUtf8()),
                      Qt::ISODate).date();

    QDate toD = QDateTime::fromString(
                    QUrl::fromPercentEncoding(toDate.toUtf8()),
                    Qt::ISODate).date();

    QString folder = QUrl::fromPercentEncoding(logDir.toUtf8());

    if (!fromD.isValid() || !toD.isValid())
        return {{"success", false}, {"error", "Invalid date"}};

    if (!QDir(folder).exists())
        return {{"success", false}, {"error", "Invalid logDir"}};

    QStringList files =
        QDir(folder).entryList({"*.bin"}, QDir::Files);

    for (const QString& file : files)
    {

        QString base = QFileInfo(file).baseName();
        QStringList parts = base.split('-');

        if (parts.size() != 3)
            continue;

        QDate fileDate(
            2000 + parts[2].toInt(),
            parts[1].toInt(),
            parts[0].toInt());

        if (!fileDate.isValid())
            continue;

        if (fileDate < fromD || fileDate > toD)
            continue;

        QFile f(folder + "/" + file);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream in(&f);

        while (!in.atEnd())
        {
            QString line = in.readLine().trimmed();
            if (line.isEmpty())
                continue;

            if (!line.startsWith("AAAA"))
                continue;

            QByteArray raw = QByteArray::fromHex(line.toLatin1());

            if (raw.size() < 20)
                continue;

            quint8 msgType = static_cast<quint8>(raw[2]);

            if (msgType != expectedMsgType)
                continue;

            QJsonObject row;

            row["file_date"] = fileDate.toString("yyyy-MM-dd");
            row["msg_type"] = JNUM(msgType);
            row["msg_length"] =
                JNUM((static_cast<quint8>(raw[3]) << 8) |
                     static_cast<quint8>(raw[4]));

            row["msg_sequence"] =
                JNUM((static_cast<quint8>(raw[5]) << 8) |
                     static_cast<quint8>(raw[6]));





            int index = 7;

            // Stationary KAVACH ID (2)
            row["stationary_kavach_id"] =
                JNUM((static_cast<quint8>(raw[index]) << 8) |
                     static_cast<quint8>(raw[index+1]));
            index += 2;

            // NMS System ID (2)
            row["nms_system_id"] =
                JNUM((static_cast<quint8>(raw[index]) << 8) |
                     static_cast<quint8>(raw[index+1]));
            index += 2;

            // System Version (1)
            row["system_version"] =
                JNUM(static_cast<quint8>(raw[index]));
            index += 1;

            // Skip Date (3) + Time (3)
            index += 6;


            quint8 eventCount = raw[index++];
            row["event_count"] = JNUM(eventCount);

            QJsonArray events;

            for (int i = 0; i < eventCount; ++i)
            {
                if (index + 1 >= raw.size())
                    break;

                quint16 eventId =
                    (static_cast<quint8>(raw[index]) << 8) |
                    static_cast<quint8>(raw[index+1]);
                index += 2;

                int dataSize =
                    (msgType == 0x17)
                        ? getEventDataSize_0x17(eventId)
                        : getEventDataSize_0x18(eventId);

                if (dataSize <= 0)
                    break;

                if (index + dataSize > raw.size())
                    break;

                QJsonObject ev;
                ev["event_id"] = JNUM(eventId);

                if (dataSize == 1)
                {
                    ev["event_data"] =
                        JNUM(static_cast<quint8>(raw[index]));
                }
                else if (dataSize == 2)
                {
                    ev["event_data"] =
                        JNUM((static_cast<quint8>(raw[index]) << 8) |
                             static_cast<quint8>(raw[index+1]));
                }
                else if (dataSize == 4)
                {
                    ev["event_data"] =
                        JNUM((static_cast<quint8>(raw[index]) << 24) |
                             (static_cast<quint8>(raw[index+1]) << 16) |
                             (static_cast<quint8>(raw[index+2]) << 8) |
                             static_cast<quint8>(raw[index+3]));
                }

                index += dataSize;

                events.append(ev);
            }


            row["events"] = events;

            rows.append(row);
        }
    }

    return {{"success", true}, {"data", rows}};
}


// =====================================================
// EVENT SIZE TABLES
// =====================================================

int BackendStationaryHealth::getEventDataSize_0x17(quint16 id)
{
    if (id >= 1 && id <= 20) return 1;
    if (id == 21) return 2;
    if (id == 22) return 1;
    if (id == 23) return 2;
    if (id == 24) return 1;
    if (id == 25 || id == 26) return 1;
    if (id >= 27 && id <= 37) return 1;
    if (id >= 38 && id <= 42) return 2;
    if (id == 43 || id == 44) return 4;
    if (id == 45) return 2;
    return 2;
}

int BackendStationaryHealth::getEventDataSize_0x18(quint16 id)
{
    if (id >= 1 && id <= 16) return 1;
    if (id == 17) return 2;
    if (id >= 18 && id <= 26) return 1;
    if (id == 27 || id == 28) return 2;
    if (id >= 29 && id <= 32) return 1;
    if (id == 33) return 2;
    if (id >= 34 && id <= 37) return 2;
    if (id == 38) return 2;
    if (id == 39) return 4;
    if (id == 40) return 4;
    if (id >= 41 && id <= 45) return 1;
    if (id == 46 || id == 47) return 3;
    if (id == 48) return 4;
    if (id >= 49 && id <= 53) return 1;
    if (id == 54) return 1;
    if (id == 55) return 2;
    if (id == 56) return 2;
    if (id == 57) return 4;
    return 2;
}
