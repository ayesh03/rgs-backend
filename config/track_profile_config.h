#pragma once

#include <QString>
#include <QMap>


struct StationRange
{
    int nominalStart;
    int nominalEnd;
    int reverseStart;
    int reverseEnd;
};

extern const QMap<QString, StationRange> STATION_RANGE_MAP;
extern const QMap<QString, QString> STATION_ID_TO_CODE;


extern const QMap<QString, StationRange> STATION_RANGE_MAP;
