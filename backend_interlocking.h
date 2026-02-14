#ifndef BACKEND_INTERLOCKING_H
#define BACKEND_INTERLOCKING_H

#include <QObject>
#include <QJsonObject>

class BackendInterlocking : public QObject
{
    Q_OBJECT

public:
    explicit BackendInterlocking(QObject *parent = nullptr)
        : QObject(parent) {}

    // =====================================================
    // 1️⃣ Get stations between FROM–TO (with time)
    // =====================================================
    QJsonObject getStationsForDateRange(
        const QString &logDir,
        const QString &fromDate,
        const QString &toDate
        );

    // =====================================================
    // 2️⃣ Generate interlocking report (FROM–TO)
    // =====================================================
    QJsonObject generateReportByDateRange(
        const QString &logDir,
        const QString &fromDate,
        const QString &toDate,
        const QString &stationCode,
        int page
        );
};

#endif // BACKEND_INTERLOCKING_H
