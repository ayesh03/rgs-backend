#ifndef DBCONFIG_H
#define DBCONFIG_H

#include <QString>

// Connection name
static const QString DB_CONN_NAME = "main_db_conn";

// SQL Server connection string
static const QString DB_CONNECTION_STRING =
    "Driver={ODBC Driver 17 for SQL Server};"
    "Server=192.168.50.131,1433;"
    "Database=rgs_db;"
    "UID=sa;"
    "PWD=Areca@123!;"
    "Encrypt=no;"
    "TrustServerCertificate=yes;";


#endif // DBCONFIG_H
