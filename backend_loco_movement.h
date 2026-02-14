#ifndef BACKEND_LOCO_MOVEMENT_H
#define BACKEND_LOCO_MOVEMENT_H

#include <QJsonObject>
#include <QString>

class BackendLocoMovement
{
public:
    static QJsonObject createTable();

    static QJsonObject insertLog(const QJsonObject &payload);


    static QJsonObject fetchByDateRange(
        const QString &fromDate,
        const QString &toDate,
        const QByteArray &fileData);



    static QJsonObject fetchLatest(int limit = 100);
    static bool fileDateInRange(const QString &filePath,
                                const QDate &from,
                                const QDate &to);
    static QDate  parseFileDate(const QString &fileName);

};

#endif
