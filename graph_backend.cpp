#include "graph_backend.h"

#include <QFile>
#include <QDir>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <iostream>
// --------------------------------------------------
// Read raw AAAA12 packets
// --------------------------------------------------
QStringList GraphBackend::readBinFile(const QString &filePath)
{
    QFile file(filePath);
    QStringList packets;

    if (!file.open(QIODevice::ReadOnly))
        return packets;

    QByteArray raw = file.readAll().toUpper();
    raw.replace("\r", "");
    raw.replace("\n", "");

    // split by AAAA12
    raw.replace("AAAA12", "|AAAA12");
    QList<QByteArray> split = raw.split('|');

    for (const QByteArray &p : split)
    {
        if (p.startsWith("AAAA12"))
            packets.append(QString::fromLatin1(p));
    }
    return packets;
}

// --------------------------------------------------
// Decode ONE loco regular packet (AAAA12, 1010)
// Desktop-accurate decoding
// --------------------------------------------------
bool GraphBackend::decodeLocoPacket(
    const QByteArray &pktHex,
    quint32 &locoId,
    quint32 &absLoc,
    quint16 &speed,
    quint8  &mode,
    quint8  &direction,
    quint32 &frameNo
    )
{
    QByteArray pkt = QByteArray::fromHex(pktHex);

    if (pkt.size() < 30)
        return false;

    // Detect payload start (A5C3 or C3)
    int payloadStart = -1;

    int a5Pos = pkt.indexOf(char(0xA5));
    if (a5Pos >= 0 && a5Pos + 1 < pkt.size() &&
        static_cast<quint8>(pkt[a5Pos + 1]) == 0xC3)
    {
        payloadStart = a5Pos + 2;   // NMS wrapped
    }
    else
    {
        int c3Pos = pkt.indexOf(char(0xC3));
        if (c3Pos >= 0)
            payloadStart = c3Pos + 1;   // Desktop format
        else
            return false;
    }

    // HARD SAFETY CHECK (prevents crash)
    if (payloadStart + 16 >= pkt.size())
        return false;

    // Convert payload to bit string (DESKTOP PARITY)
    QString bits;
    for (int i = payloadStart; i < pkt.size() - 4; ++i) // skip CRC
    {
        quint8 b = static_cast<quint8>(pkt[i]);
        for (int k = 7; k >= 0; --k)
            bits.append((b >> k) & 1 ? '1' : '0');
    }

    if (bits.size() < 123)
        return false;

    // Decode fields (same as desktop)
    quint8 pktType = bits.mid(0,4).toUInt(nullptr,2);
    if (pktType != 0x0A)
        return false;

    frameNo = bits.mid(11,17).toUInt(nullptr,2);
    locoId  = bits.mid(28,20).toUInt(nullptr,2);
    absLoc  = bits.mid(51,23).toUInt(nullptr,2);
    speed   = bits.mid(105,9).toUInt(nullptr,2);
    direction = bits.mid(114,2).toUInt(nullptr,2);
    mode      = bits.mid(119,4).toUInt(nullptr,2);

    if (locoId == 0 || locoId == 0xFFFFF)
        return false;

    return true;
}

// --------------------------------------------------
// Graph META API
// --------------------------------------------------
QJsonObject GraphBackend::getGraphMeta(
    const QString &logDir,
    const QString &fromDate,
    const QString &toDate
    )
{
    QSet<QString> locoWithGraphData;

    QSet<QString> locoSet;
    QSet<QString> dirSet;
    QSet<QString> dateSet;

    QDate from = QDate::fromString(fromDate.left(10), "yyyy-MM-dd");
    QDate to   = QDate::fromString(toDate.left(10), "yyyy-MM-dd");

    if (!from.isValid() || !to.isValid())
    {
        return {
            {"success", false},
            {"error", "Invalid from/to date"}
        };
    }

    QDir dir(logDir);
    QStringList files = dir.entryList({"*.bin"}, QDir::Files);

    for (const QString &f : files)
    {
        QString base = f;
        base.chop(4);

        QDate fileDate = QDate::fromString(base, "dd-MM-yy");
        if (fileDate.isValid() && fileDate.year() < 2000)
            fileDate = fileDate.addYears(100);

        if (!fileDate.isValid() || fileDate < from || fileDate > to)
            continue;

        dateSet.insert(fileDate.toString("yyyy-MM-dd"));

        QStringList packets = readBinFile(logDir + "/" + f);

        // REMOVED the locoCaptured flag - now captures ALL locos
        bool locoSelectedForFile = false;

        for (const QString &pkt : packets)
        {
            quint32 loco, absLoc, frame;
            quint16 speed;
            quint8 dirVal, mode;

            if (!decodeLocoPacket(pkt.toLatin1(), loco, absLoc, speed, mode, dirVal, frame))
                continue;

            bool hasGraphValue =
                (absLoc > 0) ||
                (speed > 0)  ||
                (mode > 0);

            if (!hasGraphValue)
                continue;

            // DESKTOP RULE: first valid loco ONLY
            if (!locoSelectedForFile)
            {
                locoWithGraphData.insert(QString::number(loco));
                locoSelectedForFile = true;
            }

            // collect directions (same as before)
            if (dirVal == 1)
                dirSet.insert("Nominal");
            else if (dirVal == 2)
                dirSet.insert("Reverse");
        }

    }
    QStringList locos = locoWithGraphData.values();

    std::sort(locos.begin(), locos.end(), [](const QString &a, const QString &b){
        return a.toUInt() < b.toUInt();
    });

    QStringList dates = dateSet.values();
    std::sort(dates.begin(), dates.end());

    QStringList dirs = dirSet.values();
    std::sort(dirs.begin(), dirs.end());




    return {
        {"success", true},
        {"locos", QJsonArray::fromStringList(locos)},
        {"dates", QJsonArray::fromStringList(dates)},
        {"directions", QJsonArray::fromStringList(dirs)},
        {"graphTypes", QJsonArray{
                           "Location Vs Speed",
                           "Time Vs Speed",
                           "Location Vs Mode",
                           "Time Vs Mode"
                       }}
    };
}


QJsonObject GraphBackend::getGraphData(
    const QString &locoIdStr,
    const QString &fromDate,
    const QString &toDate,
    const QString &directionStr,
    const QString &,
    const QString &graphType,
    const QString &logDir
    )
{
    quint32 targetLoco = locoIdStr.toUInt();



    QJsonArray xArr, yArr;
    bool hasData = false;
    int totalPackets = 0;
    int decodedPackets = 0;
    int matchedLoco = 0;
    int matchedDirection = 0;

    QDate from = QDate::fromString(fromDate.left(10), "yyyy-MM-dd");
    QDate to   = QDate::fromString(toDate.left(10), "yyyy-MM-dd");



    for (QDate d = from; d <= to; d = d.addDays(1))
    {
        QString file = logDir + "/" + d.toString("dd-MM-yy") + ".bin";


        QStringList packets = readBinFile(file);
        totalPackets += packets.size();



        for (const QString &pkt : packets)
        {
            quint32 loco, loc, frame;
            quint16 speed;
            quint8 dirVal, mode;

            if (!decodeLocoPacket(pkt.toLatin1(), loco, loc, speed, mode, dirVal, frame))
                continue;

            decodedPackets++;

            if (loco != targetLoco)
            {
                // std::cout << "  Skipped: Loco mismatch (" << loco
                //           << " != " << targetLoco << ")" << std::endl;
                continue;
            }

            matchedLoco++;

            QString dir;
            switch (dirVal)
            {
            case 1: dir = "Nominal"; break;
            case 2: dir = "Reverse"; break;
            default: dir = "Unidentified"; break;
            }

            if (dir != directionStr)
            {

                continue;
            }

            matchedDirection++;

            if (graphType == "Location Vs Speed")
            {
                xArr.append((int)loc);
                yArr.append((int)speed);

            }
            else if (graphType == "Time Vs Speed")
            {
                xArr.append((int)frame);
                yArr.append((int)speed);

            }
            else if (graphType == "Location Vs Mode")
            {
                xArr.append((int)loc);
                yArr.append((int)mode);

            }
            else if (graphType == "Time Vs Mode")
            {
                xArr.append((int)frame);
                yArr.append((int)mode);

            }

            hasData = true;
        }
    }

    // std::cout << "\n========== SUMMARY ==========" << std::endl;
    // std::cout << "Total packets read: " << totalPackets << std::endl;
    // std::cout << "Successfully decoded: " << decodedPackets << std::endl;
    // std::cout << "Matched loco: " << matchedLoco << std::endl;
    // std::cout << "Matched direction: " << matchedDirection << std::endl;
    // std::cout << "Data points added: " << xArr.size() << std::endl;
    // std::cout << "=============================" << std::endl;

    QJsonObject data;
    data["x"] = xArr;
    data["y"] = yArr;

    QJsonObject res;
    res["success"] = true;
    res["graphType"] = graphType;
    res["data"] = data;
    res["hasData"] = hasData;

    return res;
}
// --------------------------------------------------
QJsonObject GraphBackend::initTables()
{
    return QJsonObject{{"success", true}};
}
