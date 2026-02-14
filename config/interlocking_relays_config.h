#ifndef INTERLOCKING_RELAYS_CONFIG_H
#define INTERLOCKING_RELAYS_CONFIG_H

#include <QString>
#include <QVector>

struct RelayInfo {
    QString serial;
    QString relay_name;
    int address;
};

// Default relay list
QVector<RelayInfo> getDefaultInterlockingRelays();

// Station-wise relay mapping
QVector<RelayInfo> getInterlockingRelaysForStation(const QString &stationCode);

// Relay count
int getInterlockingRelayCount(const QString &stationCode);

#endif // INTERLOCKING_RELAYS_CONFIG_H
