#include "backend_db.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QDir>
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

QJsonObject BackendDB::checkConnection()
{
    QJsonObject result;

     QSqlDatabase db = openDatabase();

    if (db.open()) {
        result["connected"] = true;
        result["driver"] = db.driverName();
        result["db"] = "SQL Server";
        db.close();
    } else {
        result["connected"] = false;
        result["error"] = db.lastError().text();
    }

    QSqlDatabase::removeDatabase("health_check");
    return result;
}
