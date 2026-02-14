#ifndef TRACK_PROFILE_REPORT_BACKEND_H
#define TRACK_PROFILE_REPORT_BACKEND_H

#include <QString>
#include <QStringList>
#include <QJsonObject>

class TrackProfileReportBackend
{
public:
    // Station master (desktop parity)
    static QJsonObject getAllStations();

    // Track Profile Report
    static QJsonObject getReport(
        const QString &fromDate,
        const QString &toDate,
        const QString &logDir,
        const QStringList &stations = {}
        );
};

#endif // TRACK_PROFILE_REPORT_BACKEND_H
