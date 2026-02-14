#include "backend_interlocking.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QUrl>
#include <QSet>

#include "stations_config.h"
#include "interlocking_relays_config.h"

// ---------------------------------------------
// Hex bitmap → binary string (desktop parity)
// ---------------------------------------------
static QString hexToBinary(const QString &hex)
{
    QString bin;
    for (int i = 0; i < hex.length(); i += 2) {
        QByteArray b = QByteArray::fromHex(hex.mid(i,2).toLatin1());
        for (unsigned char c : b)
            bin += QString::number(c, 2).rightJustified(8, '0');
    }
    return bin;
}

// ---------------------------------------------
// Reverse byte order (LSB swap)
// ---------------------------------------------
static QString reverseBytes(const QString &hex)
{
    QString out;
    for (int i = 0; i < hex.length(); i += 2)
        out = hex.mid(i,2) + out;
    return out;
}


    // =====================================================
    // Helpers
    // =====================================================
    static QDateTime parseDateTime(const QString &s)
{
    QString str = QUrl::fromPercentEncoding(s.toUtf8());
    str.replace("T", " ");
    str = str.trimmed();

    QDateTime dt;
    dt = QDateTime::fromString(str, "yyyy-MM-dd HH:mm:ss");
    if (dt.isValid()) return dt;

    dt = QDateTime::fromString(str, "yyyy-MM-dd HH:mm");
    if (dt.isValid()) return dt;

    dt = QDateTime::fromString(str, Qt::ISODate);
    if (dt.isValid()) return dt;

    return {};
}

static QStringList readBinLines(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&f);
    QString raw = in.readAll().toUpper().trimmed();

    raw.replace("AAAA15", "\nAAAA15");
    raw.replace("AAAA16", "\nAAAA16");

    QStringList packets;
    for (const QString &line : raw.split("\n", Qt::SkipEmptyParts)) {
        QString spaced;
        for (int i = 0; i < line.length(); i += 2)
            spaced += line.mid(i, 2) + " ";
        packets.append(spaced.trimmed());
    }
    return packets;
}

static QDate parseFileDate(const QString &fileName)
{
    QString base = QFileInfo(fileName).baseName(); // dd-MM-yy
    QStringList p = base.split("-");
    if (p.size() != 3) return {};
    return QDate(2000 + p[2].toInt(), p[1].toInt(), p[0].toInt());
}

// =====================================================
// 1️⃣ GET STATIONS (FROM–TO)
// =====================================================
QJsonObject BackendInterlocking::getStationsForDateRange(
    const QString &logDir,
    const QString &fromDate,
    const QString &toDate)
{
    QString folder = QUrl::fromPercentEncoding(logDir.toUtf8());
    QDateTime fromDt = parseDateTime(fromDate);
    QDateTime toDt   = parseDateTime(toDate);

    if (!fromDt.isValid() || !toDt.isValid())
        return {{"success", false}, {"error", "Invalid from/to date format"}};

    QSet<QString> stations;

    for (const QString &file : QDir(folder).entryList({"*.bin"}, QDir::Files)) {
        QDate fileDate = parseFileDate(file);
        if (!fileDate.isValid() || fileDate < fromDt.date() || fileDate > toDt.date())
            continue;

        for (const QString &pkt : readBinLines(folder + "/" + file)) {
            if (!pkt.startsWith("AA AA 15")) continue;

            QStringList f = pkt.split(" ");
            if (f.size() < 18) continue;

            QDateTime pktDt(
                QDate(2000 + f[14].toInt(nullptr,16),
                      f[13].toInt(nullptr,16),
                      f[12].toInt(nullptr,16)),
                QTime(f[15].toInt(nullptr,16),
                      f[16].toInt(nullptr,16),
                      f[17].toInt(nullptr,16))
                );

            if (pktDt < fromDt || pktDt > toDt) continue;

            bool ok;
            int stnId = (f[7] + f[8]).toInt(&ok,16);
            if (!ok) continue;

            StationInfo s;
            if (getStationById(stnId, s))
                stations.insert(s.station_code);
        }
    }

    QJsonArray out;
    for (const QString &s : stations) out.append(s);
    return {{"success", true}, {"data", out}};
}

// =====================================================
// GENERATE REPORT (AAAA15 + AAAA16)
// =====================================================
QJsonObject BackendInterlocking::generateReportByDateRange(
    const QString &logDir,
    const QString &fromDate,
    const QString &toDate,
    const QString &stationCode,
    int page)
{
    QString folder = QUrl::fromPercentEncoding(logDir.toUtf8());
    QDateTime fromDt = parseDateTime(fromDate);
    QDateTime toDt   = parseDateTime(toDate);

    if (!fromDt.isValid() || !toDt.isValid())
        return {{"success", false}, {"error", "Invalid from/to date format"}};

    const auto relays = getInterlockingRelaysForStation(stationCode);
    const int PAGE_SIZE = 5000;

    QJsonArray rows;
    int totalRows = 0;

    for (const QString &file : QDir(folder).entryList({"*.bin"}, QDir::Files)) {

        QDate fileDate = parseFileDate(file);
        if (!fileDate.isValid() || fileDate < fromDt.date() || fileDate > toDt.date())
            continue;

        for (const QString &pkt : readBinLines(folder + "/" + file)) {

            QStringList f = pkt.split(" ");
            if (f.size() < 20) continue;

            // ================= AAAA15 =================
            if (f[0]=="AA" && f[1]=="AA" && f[2]=="15") {

                bool ok;
                int stnId = (f[7] + f[8]).toInt(&ok,16);
                StationInfo s;
                if (!ok || !getStationById(stnId,s) || s.station_code!=stationCode)
                    continue;

                QDateTime pktDt(
                    QDate(2000+f[14].toInt(nullptr,16),
                          f[13].toInt(nullptr,16),
                          f[12].toInt(nullptr,16)),
                    QTime(f[15].toInt(nullptr,16),
                          f[16].toInt(nullptr,16),
                          f[17].toInt(nullptr,16))
                    );
                if (pktDt < fromDt || pktDt > toDt) continue;

                int frame =
                    pktDt.time().hour()*3600 +
                    pktDt.time().minute()*60 +
                    pktDt.time().second() + 1;

                // Extract relay bitmap hex
                QString bitmapHex;
                for (int i = 21; i < f.size(); ++i)
                    bitmapHex += f[i];

                // Desktop: byte swap → binary
                QString swapped = reverseBytes(bitmapHex);
                QString binData = hexToBinary(swapped);

                for (int i = 0; i < relays.size() && i < binData.length(); ++i) {
                    totalRows++;
                    if (totalRows <= (page-1)*PAGE_SIZE || totalRows > page*PAGE_SIZE)
                        continue;

                    bool isTPR = relays[i].relay_name.endsWith("_TPR");
                    QChar bit = binData[i];

                    QString status;
                    if (isTPR)
                        status = (bit == '0') ? "Picked Up" : "Drop Down";
                    else
                        status = (bit == '1') ? "Picked Up" : "Drop Down";

                    QJsonObject r;
                    r["date"]    = pktDt.date().toString("yyyy-MM-dd");
                    r["time"]    = pktDt.time().toString("HH:mm:ss");
                    r["frameNo"] = QString::number(frame);
                    r["station"] = stationCode;
                    r["relay"]   = relays[i].relay_name;
                    r["serial"]  = relays[i].serial;
                    r["status"]  = status;

                    rows.append(r);
                }

            }

            // ================= AAAA16 =================
            else if (f[0]=="AA" && f[1]=="AA" && f[2]=="16") {

                bool ok;
                int stnId = (f[7] + f[8]).toInt(&ok,16);
                StationInfo s;
                if (!ok || !getStationById(stnId,s) || s.station_code!=stationCode)
                    continue;

                QDateTime pktDt(
                    QDate(2000+f[14].toInt(nullptr,16),
                          f[13].toInt(nullptr,16),
                          f[12].toInt(nullptr,16)),
                    QTime(f[15].toInt(nullptr,16),
                          f[16].toInt(nullptr,16),
                          f[17].toInt(nullptr,16))
                    );
                if (pktDt < fromDt || pktDt > toDt) continue;

                int frame =
                    pktDt.time().hour()*3600 +
                    pktDt.time().minute()*60 +
                    pktDt.time().second() + 1;

                int eventCount = f[18].toInt(nullptr,16);

                for (int i = 0; i < eventCount && (21 + 3*i + 1) < f.size(); ++i) {

                    // Relay address (2 bytes)
                    QString relayAddrHex = f[19 + 3*i] + f[20 + 3*i];
                    QString statusHex    = f[21 + 3*i];

                    bool ok;
                    int relayAddr = relayAddrHex.toInt(&ok, 16);
                    if (!ok) continue;

                    auto it = std::find_if(
                        relays.begin(),
                        relays.end(),
                        [&](const RelayInfo &r) {
                            return r.address == relayAddr;
                        }
                        );


                    if (it == relays.end())
                        continue;

                    totalRows++;
                    if (totalRows <= (page-1)*PAGE_SIZE || totalRows > page*PAGE_SIZE)
                        continue;

                    QJsonObject r;
                    r["date"]    = pktDt.date().toString("yyyy-MM-dd");
                    r["time"]    = pktDt.time().toString("HH:mm:ss");
                    r["frameNo"] = QString::number(frame);
                    r["station"] = stationCode;
                    r["relay"]   = it->relay_name;
                    r["serial"]  = it->serial;
                    bool isTPR = it->relay_name.endsWith("_TPR");

                    QString status;
                    if (isTPR)
                        status = (statusHex == "01") ? "Drop Down" : "Picked Up";
                    else
                        status = (statusHex == "01") ? "Picked Up" : "Drop Down";

                    r["status"] = status;


                    rows.append(r);
                }

            }
        }
    }

    int totalPages = qMax(1, (totalRows + PAGE_SIZE - 1) / PAGE_SIZE);
    return {
        {"success", true},
        {"data", rows},
        {"totalRows", totalRows},
        {"totalPages", totalPages},
        {"page", page}
    };
}
