#include "backend_gprs_fault.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QJsonArray>
#include <iostream>
#include "dbconfig.h"
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
// =====================================================
// CREATE TABLE
// =====================================================
QJsonObject BackendGPRSFault::initTable()
{
    QSqlDatabase db = openDatabase();
    if (!db.open())
        return {{"success", false}, {"error", db.lastError().text()}};

    QSqlQuery q(db);
    q.exec("DROP TABLE IF EXISTS gprs_fault_logs");

    bool ok = q.exec(
        "CREATE TABLE gprs_fault_logs ("
        "id BIGINT IDENTITY(1,1) PRIMARY KEY,"
        "event_time DATETIME NOT NULL,"
        "loco_id VARCHAR(32),"
        "station_code VARCHAR(16),"
        "fault_code VARCHAR(32),"
        "description NVARCHAR(255),"
        "status VARCHAR(16) CHECK (status IN ('ACTIVE','CLEARED')),"
        "created_at DATETIME DEFAULT GETDATE()"
        ")"
        );

    db.close();
    QSqlDatabase::removeDatabase("gprs_fault_init");

    return {{"success", ok}};
}

// =====================================================
// FETCH FAULTS
// =====================================================
QJsonObject BackendGPRSFault::getFaults(
    const QString &station,
    int page,
    int limit
    )
{
    QJsonArray rows;
    QSqlDatabase db = openDatabase();

    if (!db.open())
        return {{"success", false}, {"error", db.lastError().text()}};

    int offset = (page - 1) * limit;

    QSqlQuery q(db);
    q.prepare(
        "SELECT event_time, loco_id, station_code, fault_code, description, status "
        "FROM gprs_fault_logs "
        "WHERE (:station IS NULL OR station_code = :station) "
        "ORDER BY event_time DESC "
        "OFFSET :offset ROWS FETCH NEXT :limit ROWS ONLY"
        );

    if (station.isEmpty())
        q.bindValue(":station", QVariant(QVariant::String));
    else
        q.bindValue(":station", station);

    q.bindValue(":offset", offset);
    q.bindValue(":limit", limit);

    if (!q.exec()) {
        db.close();
        return {{"success", false}, {"error", q.lastError().text()}};
    }

    QSqlRecord rec = q.record();
    while (q.next()) {
        QJsonObject row;
        row["event_time"]   = q.value("event_time").toString();
        row["loco_id"]      = q.value("loco_id").toString();
        row["station_code"] = q.value("station_code").toString();
        row["fault_code"]   = q.value("fault_code").toString();
        row["description"]  = q.value("description").toString();
        row["status"]       = q.value("status").toString();
        rows.append(row);
    }

    db.close();
    QSqlDatabase::removeDatabase("gprs_fault_fetch");

    return {
        {"success", true},
        {"data", rows},
        {"page", page},
        {"limit", limit}
    };
}

// =====================================================
// FETCH FAULTS BY DATE RANGE
// =====================================================
QJsonObject BackendGPRSFault::getFaultsByDateRange(
    const QString &station,
    const QString &fromDate,
    const QString &toDate,
    int page,
    int limit
    )
{

    QJsonArray rows;
    QSqlDatabase db = openDatabase();;

    if (!db.open()) {
        return {{"success", false}, {"error", db.lastError().text()}};
    }

    // -------- Decode & normalize --------
    QString fromStr = fromDate;
    QString toStr   = toDate;

    fromStr = QUrl::fromPercentEncoding(fromStr.toUtf8());
    toStr   = QUrl::fromPercentEncoding(toStr.toUtf8());

    fromStr.replace("T", " ");
    toStr.replace("T", " ");

    // -------- Robust parser --------
    auto parseDate = [](const QString &s) -> QDateTime {
        QDateTime dt;

        dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
        if (dt.isValid()) return dt;

        dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm");
        if (dt.isValid()) return dt;

        dt = QDateTime::fromString(s, Qt::ISODate);
        if (dt.isValid()) return dt;

        return QDateTime();
    };

    QDateTime fromDt = parseDate(fromStr);
    QDateTime toDt   = parseDate(toStr);

    if (!fromDt.isValid() || !toDt.isValid()) {
        db.close();
        QSqlDatabase::removeDatabase("gprs_fault_range");
        return {
            {"success", false},
            {"error", "Invalid date format received from frontend"}
        };
    }

    // Normalize seconds
    fromDt.setTime(QTime(fromDt.time().hour(), fromDt.time().minute(), 0));
    toDt.setTime(QTime(toDt.time().hour(), toDt.time().minute(), 59));

    int offset = (page - 1) * limit;

    QSqlQuery q(db);
    q.prepare(
        "SELECT event_time, loco_id, station_code, fault_code, description, status "
        "FROM gprs_fault_logs "
        "WHERE (:station IS NULL OR station_code = :station) "
        "AND event_time >= :fromDt AND event_time <= :toDt "
        "ORDER BY event_time DESC "
        "OFFSET :offset ROWS FETCH NEXT :limit ROWS ONLY"
        );

    if (station.isEmpty())
        q.bindValue(":station", QVariant(QVariant::String));
    else
        q.bindValue(":station", station);

    q.bindValue(":fromDt", fromDt);
    q.bindValue(":toDt", toDt);
    q.bindValue(":offset", offset);
    q.bindValue(":limit", limit);

    if (!q.exec()) {
        db.close();
        QSqlDatabase::removeDatabase("gprs_fault_range");
        return {{"success", false}, {"error", q.lastError().text()}};
    }

    while (q.next()) {
        QJsonObject row;
        row["event_time"]   = q.value("event_time").toString();
        row["loco_id"]      = q.value("loco_id").toString();
        row["station_code"] = q.value("station_code").toString();
        row["fault_code"]   = q.value("fault_code").toString();
        row["description"]  = q.value("description").toString();
        row["status"]       = q.value("status").toString();
        rows.append(row);
    }

    db.close();
    QSqlDatabase::removeDatabase("gprs_fault_range");


    return {
        {"success", true},
        {"data", rows},
        {"page", page},
        {"limit", limit}
    };
}

// =====================================================
// INSERT FAULT
// =====================================================
QJsonObject BackendGPRSFault::insertFault(const QJsonObject &p)
{
    QSqlDatabase db = openDatabase();;
    if (!db.open())
        return {{"success", false}, {"error", db.lastError().text()}};

    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO gprs_fault_logs ("
        "event_time, loco_id, station_code, fault_code, description, status"
        ") VALUES (?, ?, ?, ?, ?, ?)"
        );

    q.addBindValue(p.value("event_time").toString());
    q.addBindValue(p.value("loco_id").toString());
    q.addBindValue(p.value("station_code").toString());
    q.addBindValue(p.value("fault_code").toString());
    q.addBindValue(p.value("description").toString());
    q.addBindValue(p.value("status").toString());

    bool ok = q.exec();
    if (!ok) {
        db.close();
        return {{"success", false}, {"error", q.lastError().text()}};
    }

    db.close();
    QSqlDatabase::removeDatabase("gprs_fault_insert");

    return {{"success", true}};
}

// =====================================================
// FETCH DISTINCT GPRS STATIONS
// =====================================================
QJsonObject BackendGPRSFault::getStations()
{
    QSqlDatabase db = openDatabase();;
    if (!db.open())
        return {{"success", false}, {"error", db.lastError().text()}};

    QSqlQuery q(db);
    QJsonArray stations;

    if (!q.exec(
            "SELECT DISTINCT station_code "
            "FROM gprs_fault_logs "
            "WHERE station_code IS NOT NULL "
            "ORDER BY station_code"
            )) {
        db.close();
        return {{"success", false}, {"error", q.lastError().text()}};
    }

    while (q.next()) {
        stations.append(q.value(0).toString());
    }

    db.close();
    QSqlDatabase::removeDatabase("gprs_stations");

    return {
        {"success", true},
        {"data", stations}
    };
}

