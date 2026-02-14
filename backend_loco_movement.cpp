#include "backend_loco_movement.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QJsonArray>
#include <iostream>
#include <QUrl>
#include "dbconfig.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QTextStream>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#undef QT_NO_DEBUG_OUTPUT
#include "lvk_pos_info_packet.h"
#include "lvk_pos_info_parser.h"

#define JNUM(x) QJsonValue(static_cast<qint64>(x))
#define JBOOL(x) QJsonValue(static_cast<bool>(x))


// =====================================================
// SAFE DATABASE OPENER (SQL SERVER)
// =====================================================
static QSqlDatabase openDatabase()
{
    if (QSqlDatabase::contains(DB_CONN_NAME)) {
        return QSqlDatabase::database(DB_CONN_NAME);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC", DB_CONN_NAME);
    db.setDatabaseName(DB_CONNECTION_STRING);

    if (!db.open()) {
        qDebug() << "DB open failed:" << db.lastError().text();
    }

    return db;
}




QJsonObject BackendLocoMovement::fetchByDateRange(
    const QString &fromDate,
    const QString &toDate,
    const QByteArray &fileData)
{
    QJsonArray rows;

    QString fromStr = QUrl::fromPercentEncoding(fromDate.toUtf8());
    QString toStr   = QUrl::fromPercentEncoding(toDate.toUtf8());

    fromStr.replace("T", " ");
    toStr.replace("T", " ");

    auto parseDate = [](const QString &s) -> QDateTime {
        QDateTime dt;
        dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
        if (dt.isValid()) return dt;
        dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm");
        if (dt.isValid()) return dt;
        dt = QDateTime::fromString(s, Qt::ISODate);
        return dt;
    };

    QDateTime fromDt = parseDate(fromStr);
    QDateTime toDt   = parseDate(toStr);

    if (!fromDt.isValid() || !toDt.isValid()) {
        return {
            {"success", false},
            {"error", "Invalid date format"}
        };
    }

    QByteArray content = fileData;
    QList<QByteArray> lines = content.split('\n');


    for (const QByteArray &lineRaw : lines)
    {
        QString line = QString::fromLatin1(lineRaw).trimmed();

        if (!line.startsWith("AAAA12"))
            continue;

        QByteArray data = QByteArray::fromHex(line.toLatin1());
        if (data.size() < 3)
            continue;

        LVKPosInfoPacket pkt;

        try {
            pkt = LVKPosInfoParser::parse(
                reinterpret_cast<const quint8*>(data.constData()),
                data.size());
        } catch (...) {
            continue;
        }

        if (!pkt.IsDateTimeValid)
            continue;

        if (pkt.PacketDateTime < fromDt ||
            pkt.PacketDateTime > toDt)
            continue;

        QJsonObject row;
        row["event_time"] = pkt.PacketDateTime.toString(Qt::ISODate);
        row["packet_type"] = JNUM(pkt.PacketType);

        row["message_sequence"]        = JNUM(pkt.MessageSequence);
        row["stationary_kavach_id"]    = JNUM(pkt.StationaryKavachId);
        row["nms_system_id"]           = JNUM(pkt.NmsSystemId);
        row["system_version"]          = JNUM(pkt.SystemVersion);
        row["active_radio"]            = JNUM(pkt.ActiveRadio);
        row["route_id"]                = JNUM(pkt.RouteId);
        row["ma_section_count"]        = JNUM(pkt.NoOfMASections);
        row["data_source"]             = "BIN";


        if (pkt.PacketType == 0xA) {
            // ===== ONBOARD =====
            const auto& m = pkt.RadioPacket;

            row["frame_number"]            = JNUM(m.FrameNumber);
            row["source_loco_id"]          = JNUM(m.SourceLocoId);
            row["source_loco_version"]     = JNUM(m.SourceLocoVersion);
            row["absolute_loco_location"]  = JNUM(m.AbsoluteLocoLocation);

            row["l_doubt_over"]            = JNUM(m.LDoubtOver);
            row["l_doubt_under"]           = JNUM(m.LDoubtUnder);
            row["train_integrity_status"]  = JNUM(m.TrainIntegrity);
            row["train_length_m"]          = JNUM(m.TrainLength);
            row["train_speed_kmph"]        = JNUM(m.TrainSpeed);

            row["movement_direction"]      = JNUM(m.MovementDir);
            row["emergency_status"]        = JNUM(m.EmergencyStatus);
            row["loco_mode"]               = JNUM(m.LocoMode);

            row["last_rfid_tag_id"]        = JNUM(m.LastRfidTag);
            row["tag_duplicate_status"]    = JNUM(m.TagDup);
            row["tag_link_information"]    = JNUM(m.TagLinkInfo);
            row["tin"]                     = JNUM(m.Tin);

            row["brake_applied_status"]    = JNUM(m.BrakeApplied);
            row["new_ma_reply_status"]     = JNUM(m.NewMaReply);
            row["last_ref_profile_number"] = JNUM(m.LastRefProfileNum);
            row["signal_override_status"]  = JNUM(m.SignalOverride);
            row["info_ack_status"]         = JNUM(m.InfoAck);

            row["onboard_health_byte_1"]   = JNUM(m.OnboardHealthBytes[0]);

        }

        else if (pkt.PacketType == 0xD) {
            // ===== ACCESS =====
            const auto& a = pkt.ARRadioPacket;

            row["packet_type"]             = JNUM(pkt.PacketType);
            row["frame_number"]            = JNUM(a.FrameNumber);
            row["source_loco_id"]          = JNUM(a.SourceLocoId);
            row["source_loco_version"]     = JNUM(a.SourceLocoVersion);

            row["absolute_loco_location"]  = JNUM(a.AbsoluteLocoLocation);
            row["train_length"]            = JNUM(a.TrainLength);
            row["train_speed"]             = JNUM(a.TrainSpeed);

            row["movement_direction"]      = JNUM(a.MovementDir);
            row["emergency_status"]        = JNUM(a.EmergencyStatus);
            row["loco_mode"]               = JNUM(a.LocoMode);

            row["approaching_station_id"]  = JNUM(a.ApprochingStationID);
            row["last_rfid_tag"]           = JNUM(a.LastRFIDTag);
            row["tin"]                     = JNUM(a.Tin);

            row["longitude"]               = JNUM(a.Longitude);
            row["latitude"]                = JNUM(a.Latitude);
            row["loco_random_number"]      = JNUM(a.LOCO_RND_NUM_RL);

        }

        row["data_source"] = "UPLOAD";
        rows.append(row);

    }

    return {{"success", true}, {"data", rows}};
}

bool BackendLocoMovement::fileDateInRange(
    const QString &filePath,
    const QDate &from,
    const QDate &to)
{
    QDate fileDate = parseFileDate(filePath);
    qDebug() << "[FILE DATE]" << fileDate << "FROM" << from << "TO" << to;
    return fileDate.isValid() && fileDate >= from && fileDate <= to;
}

QDate BackendLocoMovement::parseFileDate(const QString &fileName) {
    // filename format: dd-MM-yy.bin
    QString base = QFileInfo(fileName).baseName(); // "05-01-26"
    QStringList parts = base.split('-');
    if (parts.size() != 3) return QDate();

    int day = parts[0].toInt();
    int month = parts[1].toInt();
    int year = 2000 + parts[2].toInt(); // add 2000 to two-digit year

    return QDate(year, month, day);
}


QJsonObject BackendLocoMovement::fetchLatest(int limit)
{
    QJsonArray rows;
    QSqlDatabase db = openDatabase();

    if (!db.open())
        return {{"success", false}, {"error", db.lastError().text()}};

    QSqlQuery q(db);
    q.exec(
        QString(
            "SELECT TOP(%1) * FROM loco_movement_logs "
            "ORDER BY event_time DESC"
            ).arg(limit)
        );
    QSqlRecord rec = q.record();
    while (q.next()) {
        QJsonObject row;
        for (int i = 0; i < rec.count(); ++i)
            row[rec.fieldName(i)] = q.value(i).toString();
        rows.append(row);
    }

    db.close();
    QSqlDatabase::removeDatabase("loco_latest");

    return {{"success", true}, {"data", rows}};
}



