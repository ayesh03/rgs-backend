#ifndef STATIONS_CONFIG_H
#define STATIONS_CONFIG_H

#include <QString>
#include <QVector>

struct StationInfo {
    int station_id;
    QString station_name;
    QString station_code;
    QString firm;
};

// Get all stations
QVector<StationInfo> getAllStations();

// Find station by code
bool getStationByCode(const QString &code, StationInfo &out);

// Find station by ID (Stationary_KAVACH_ID mapping later)
bool getStationById(int stationId, StationInfo &out);

#endif // STATIONS_CONFIG_H
