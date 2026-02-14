#include "stations_config.h"

static const QVector<StationInfo> STATIONS = {

{1037, "SECUNDERABAD", "SFM", "Areca"},
    {1038, "SITAFALMANDI(SFM)", "FAL", "Areca"},
    {1039, "KACHEGUDA", "BUD", "Areca"},
    {1040, "FALAKNUMA", "TMX-SHNR", "Areca"},
    {1041, "BUDVEL", "BDL", "Areca"},
    {1042, "UMDANAGAR", "UMD", "Areca"},
    {1072, "JUKAL", "JUL", "Areca"},
    {1044, "TIMMAPUR", "TIM", "Areca"},
    {1073, "LC23A (TMX-SHNR)", "TMX-SHNR", "Areca"},
    {1045, "SHADNAGAR", "SHAD", "Areca"},
    {1047, "BALANAGAR", "BLN", "Areca"},
    {1048, "LC39(BABR-GLY)", "BABR-GLY", "Areca"},
    {1049, "GOLLAPALLI", "GOLPAL", "Areca"},
    {1050, "JADCHERLA", "JADCHERLA", "Areca"},
    {1051, "DIVITPALLI", "DIVI", "Areca"},
    {1052, "MAHBUBNAGAR", "MAHNAGAR", "Areca"},
    {1054, "MANYAMKONDA", "MANYAM", "Areca"},
    {1055, "DEVARAKADRA JN", "DEVARKAD", "Areca"},
    {1056, "KAUKUNTLA", "KAUKUN", "Areca"},
    {1057, "WANAPARTHI ROAD", "WANAP", "Areca"},
    {601,  "ARECA A(ARC AMSA)", "ARC_AMSA", "Areca"},
    {602,  "ARECA B(ARC AMSB)", "ARC_AMSB", "Areca"}
};

QVector<StationInfo> getAllStations()
{
    return STATIONS;
}

bool getStationByCode(const QString &code, StationInfo &out)
{
    for (const auto &s : STATIONS) {
        if (s.station_code == code) {
            out = s;
            return true;
        }
    }
    return false;
}

bool getStationById(int stationId, StationInfo &out)
{
    for (const auto &s : STATIONS) {
        if (s.station_id == stationId) {
            out = s;
            return true;
        }
    }
    return false;
}
