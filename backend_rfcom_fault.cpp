#include "backend_rfcom_fault.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QJsonArray>
#include <iostream>
#include <QUrl>
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
QJsonObject BackendRFCOMFault::initTable()
{
    QSqlDatabase db = openDatabase();
    if (!db.open())
        return {{"success", false}, {"error", db.lastError().text()}};

    QSqlQuery q(db);
    q.exec("DROP TABLE IF EXISTS rfcom_fault_logs");

    bool ok = q.exec(
        "CREATE TABLE rfcom_fault_logs ("
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
    QSqlDatabase::removeDatabase("rfcom_init");

    return {{"success", ok}};
}

// =====================================================
// FETCH RFCOM FAULTS
// =====================================================
QJsonObject BackendRFCOMFault::getFaults(
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
        "SELECT event_time, loco_id, station_code, "
        "fault_code, description, status "
        "FROM rfcom_fault_logs "
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

    while (q.next()) {
        QJsonObject row;
        const QString ts = q.value("event_time").toString();
        const QStringList parts = ts.split(" ");

        row["date"]      = parts.value(0);
        row["time"]      = parts.value(1);
        row["locoId"]    = q.value("loco_id").toString();
        row["station"]   = q.value("station_code").toString();
        row["faultCode"] = q.value("fault_code").toString();
        row["description"] = q.value("description").toString();
        row["status"]    = q.value("status").toString();

        rows.append(row);
    }

    db.close();
    QSqlDatabase::removeDatabase("rfcom_fetch");

    return {
        {"success", true},
        {"data", rows},
        {"page", page},
        {"limit", limit}
    };
}

QJsonObject BackendRFCOMFault::getFaultsByDateRange(
    const QString &station,
    const QString &fromDate,
    const QString &toDate
    )
{
    QJsonArray rows;
    QSqlDatabase db = openDatabase();

    if (!db.open()) {
        return {{"success", false}, {"error", db.lastError().text()}};
    }

    // ---- Decode & normalize ----
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
        if (dt.isValid()) return dt;

        return QDateTime();
    };

    QDateTime fromDt = parseDate(fromStr);
    QDateTime toDt   = parseDate(toStr);

    if (!fromDt.isValid() || !toDt.isValid()) {
        db.close();
        // QSqlDatabase::removeDatabase("rfcom_range");
        return {
            {"success", false},
            {"error", "Invalid date format received from frontend"}
        };
    }

    // Normalize seconds
    fromDt.setTime(QTime(fromDt.time().hour(), fromDt.time().minute(), 0));
    toDt.setTime(QTime(toDt.time().hour(), toDt.time().minute(), 59));

    QSqlQuery q(db);
    q.prepare(
        "SELECT event_time, loco_id, station_code, fault_code, description, status "
        "FROM rfcom_fault_logs "
        "WHERE station_code = ? "
        "AND event_time BETWEEN ? AND ? "
        "ORDER BY event_time DESC"
        );

    q.addBindValue(station);
    q.addBindValue(fromDt);
    q.addBindValue(toDt);

    if (!q.exec()) {
        db.close();
        // QSqlDatabase::removeDatabase("rfcom_range");
        return {{"success", false}, {"error", q.lastError().text()}};
    }

    while (q.next()) {
        const QString ts = q.value("event_time").toString();
        const QStringList parts = ts.split(" ");

        QJsonObject row;
        row["date"]        = parts.value(0);
        row["time"]        = parts.value(1);
        row["locoId"]      = q.value("loco_id").toString();
        row["station"]     = q.value("station_code").toString();
        row["faultCode"]   = q.value("fault_code").toString();
        row["description"] = q.value("description").toString();
        row["status"]      = q.value("status").toString();

        rows.append(row);
    }

    db.close();
    // QSqlDatabase::removeDatabase("rfcom_range");

    return {
        {"success", true},
        {"data", rows}
    };
}


// =====================================================
// INSERT RFCOM FAULT
// =====================================================
QJsonObject BackendRFCOMFault::insertFault(const QJsonObject &p)
{
    QSqlDatabase db = openDatabase();
    if (!db.open())
        return {{"success", false}, {"error", db.lastError().text()}};

    QSqlQuery q(db);
    q.prepare(
        "INSERT INTO rfcom_fault_logs "
        "(event_time, loco_id, station_code, "
        "fault_code, description, status) "
        "VALUES (?, ?, ?, ?, ?, ?)"
        );

    q.addBindValue(p.value("event_time").toString());
    q.addBindValue(p.value("loco_id").toString());
    q.addBindValue(p.value("station_code").toString());
    q.addBindValue(p.value("fault_code").toString());
    q.addBindValue(p.value("description").toString());
    q.addBindValue(p.value("status").toString());

    bool ok = q.exec();
    db.close();
    QSqlDatabase::removeDatabase("rfcom_insert");

    return ok ? QJsonObject{{"success", true}}
              : QJsonObject{{"success", false}, {"error", q.lastError().text()}};
}
// =====================================================
// FETCH DISTINCT RFCOM STATIONS
// =====================================================
QJsonObject BackendRFCOMFault::getStations()
{
    QSqlDatabase db = openDatabase();
    if (!db.open())
        return {{"success", false}, {"error", db.lastError().text()}};

    QSqlQuery q(db);
    QJsonArray stations;

    if (!q.exec(
            "SELECT DISTINCT station_code "
            "FROM rfcom_fault_logs "
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
    QSqlDatabase::removeDatabase("rfcom_stations");

    return {
        {"success", true},
        {"data", stations}
    };
}
