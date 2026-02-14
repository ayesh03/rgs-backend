#include "backend_fault_summary.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QMap>
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
// FETCH FAULT SUMMARY (Station-wise Aggregation)
// =====================================================
QJsonObject BackendFaultSummary::getSummary()
{
    QSqlDatabase db = openDatabase();
    if (!db.open())
        return {{"success", false}, {"error", db.lastError().text()}};

    QMap<QString, QJsonObject> summaryMap;

    auto processQuery = [&](const QString &sql, const QString &keyName) {
        QSqlQuery q(db);
        if (!q.exec(sql)) {
            return false;
        }

        while (q.next()) {
            QString station = q.value("station_code").toString();
            int count = q.value("cnt").toInt();

            if (!summaryMap.contains(station)) {
                summaryMap[station] = {
                    {"station", station},
                    {"locoFaults", 0},
                    {"gprsFaults", 0},
                    {"rfcomFaults", 0},
                    {"total", 0}
                };
            }

            summaryMap[station][keyName] = count;
        }
        return true;
    };

    // Loco faults
    if (!processQuery(
            "SELECT station_code, COUNT(*) AS cnt "
            "FROM loco_fault_logs "
            "WHERE station_code IS NOT NULL "
            "GROUP BY station_code",
            "locoFaults"))
    {
        db.close();
        return {{"success", false}, {"error", "Loco summary query failed"}};
    }

    // GPRS faults
    if (!processQuery(
            "SELECT station_code, COUNT(*) AS cnt "
            "FROM gprs_fault_logs "
            "WHERE station_code IS NOT NULL "
            "GROUP BY station_code",
            "gprsFaults"))
    {
        db.close();
        return {{"success", false}, {"error", "GPRS summary query failed"}};
    }

    // RFCOM faults
    if (!processQuery(
            "SELECT station_code, COUNT(*) AS cnt "
            "FROM rfcom_fault_logs "
            "WHERE station_code IS NOT NULL "
            "GROUP BY station_code",
            "rfcomFaults"))
    {
        db.close();
        return {{"success", false}, {"error", "RFCOM summary query failed"}};
    }

    // Build final response
    QJsonArray result;
    for (auto it = summaryMap.begin(); it != summaryMap.end(); ++it) {
        QJsonObject obj = it.value();

        int total =
            obj["locoFaults"].toInt() +
            obj["gprsFaults"].toInt() +
            obj["rfcomFaults"].toInt();

        obj["total"] = total;
        result.append(obj);
    }

    db.close();
    QSqlDatabase::removeDatabase("fault_summary");

    return {
        {"success", true},
        {"data", result}
    };
}
