#ifndef BACKEND_DATABASE_H
#define BACKEND_DATABASE_H

#include <QJsonObject>
#include <QString>

class BackendDatabase
{
public:
    static QJsonObject createLocoWiseTable();
    static QJsonObject insertLocoWiseSample();
    static QJsonObject getLocoWiseReport();
    static QJsonObject createUsersTable();
    static QJsonObject insertDefaultUser();
    static QJsonObject loginUser(const QString &username, const QString &password);



};

#endif
