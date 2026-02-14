#ifndef BACKEND_DB_H
#define BACKEND_DB_H

#include <QJsonObject>

class BackendDB
{
public:
    static QJsonObject checkConnection();
};

#endif
