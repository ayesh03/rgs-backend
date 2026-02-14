#include "backend_database.h"
#include <QJsonObject>
#include <QJsonArray>

// =====================================================
// HARD CODED LOGIN
// =====================================================

QJsonObject BackendDatabase::loginUser(
    const QString &username,
    const QString &password)
{
    // Hardcoded credentials
    if (username == "admin" && password == "admin123")
    {
        return {
            {"success", true},
            {"username", "admin"},
            {"role", "ADMIN"}
        };
    }

    return {
        {"success", false},
        {"error", "Invalid username or password"}
    };
}

// =====================================================
// REMOVE ALL OTHER DB FUNCTIONS
// =====================================================

QJsonObject BackendDatabase::createUsersTable()
{
    return {{"success", true}};
}

QJsonObject BackendDatabase::insertDefaultUser()
{
    return {{"success", true}};
}



QJsonObject BackendDatabase::createLocoWiseTable()
{
    return {{"success", true}};
}

QJsonObject BackendDatabase::insertLocoWiseSample()
{
    return {{"success", true}};
}

QJsonObject BackendDatabase::getLocoWiseReport()
{
    return {
        {"success", true},
        {"data", QJsonArray()}
    };
}
