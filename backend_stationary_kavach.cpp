#include "backend_stationary_kavach.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTime>
#include <iostream>

#define JNUM(x) QJsonValue(static_cast<qint64>(x))

// =====================================================
// PUBLIC API WRAPPERS
// =====================================================

QJsonObject BackendStationaryKavach::fetchRegular(
    const QString& fromDate,
    const QString& toDate,
    const QByteArray& fileData)
{
    return processBinFiles(fromDate, toDate, fileData, 0b1001);

}

QJsonObject BackendStationaryKavach::fetchAccess(
    const QString& fromDate,
    const QString& toDate,
    const QByteArray& fileData)
{
    return processBinFiles(fromDate, toDate, fileData, 0b1011);


}

QJsonObject BackendStationaryKavach::fetchEmergency(
    const QString& fromDate,
    const QString& toDate,
    const QByteArray& fileData)
{

    return processBinFiles(fromDate, toDate, fileData, 0b1100);

}

// =====================================================
// CORE BIN PROCESSOR – DEBUG VERSION
// =====================================================

QJsonObject BackendStationaryKavach::processBinFiles(
    const QString& fromDate,
    const QString& toDate,
    const QByteArray& fileData,
    int expectedPktTypeBits)

{
    QJsonArray rows;


    QString decodedFrom = QUrl::fromPercentEncoding(fromDate.toUtf8()).trimmed();
    QString decodedTo   = QUrl::fromPercentEncoding(toDate.toUtf8()).trimmed();

    QDateTime fromDt = QDateTime::fromString(decodedFrom, Qt::ISODate);
    QDateTime toDt   = QDateTime::fromString(decodedTo, Qt::ISODate);

    if (!fromDt.isValid() || !toDt.isValid())
        return {{"success", false}, {"error", "Invalid date"}};



        QByteArray copy = fileData;
QTextStream in(&copy);



        while (!in.atEnd())
        {
            QString line = in.readLine().trimmed();

            if (line.isEmpty())
                continue;

            if (!line.startsWith("AAAA11"))
                continue;


            QByteArray raw;
            try {
                raw = QByteArray::fromHex(line.toLatin1());
                // std::cout << "RAW BYTES (" << raw.size() << "): ";
                // for (uchar b : raw)
                //     // std::cout << QString("%1 ").arg(b, 2, 16, QLatin1Char('0')).toStdString();
                // std::cout << "\n";

            } catch (...) {
                std::cout << "HEX DECODE FAILED\n";
                continue;
            }

            int idx = raw.indexOf(char(0xA5));
            if (idx < 0 || idx + 1 >= raw.size()) {
                std::cout << "A5 NOT FOUND\n";
                continue;
            }

            if ((uchar)raw[idx + 1] != 0xC3) {
                std::cout << "A5 FOUND BUT NEXT BYTE NOT C3\n";
                continue;
            }

            QByteArray payload = raw.mid(idx + 2);
            // std::cout << "PAYLOAD BYTES (" << payload.size() << "): ";
            // for (uchar b : payload)
            //     std::cout << QString("%1 ").arg(b, 2, 16, QLatin1Char('0')).toStdString();
            // std::cout << "\n";

            QString binary;
            for (uchar b : payload)
                binary += QString("%1").arg(b, 8, 2, QLatin1Char('0'));
            // std::cout << "FULL BINARY (" << binary.size() << " bits):\n";
            // for (int i = 0; i < binary.size(); i += 8) {
            //     std::cout << "[" << i << "-" << i+7 << "] "
            //               << binary.mid(i, 8).toStdString() << "\n";
            // }

            if (binary.size() < 32) {
                std::cout << "BINARY TOO SMALL\n";
                continue;
            }

            int pktType = binary.mid(0, 4).toInt(nullptr, 2);

            // Only filter for Regular.
            // Allow Access and Emergency to pass through.
            if (expectedPktTypeBits == 0b1001 && pktType != 0b1001)
                continue;

            if (expectedPktTypeBits == 0b1011 && pktType != 0b1011)
                continue;

            if (expectedPktTypeBits == 0b1100 && pktType != 0b1100)
                continue;


            const quint8* d =
                reinterpret_cast<const quint8*>(raw.constData());

            int hh = d[15];
            int mm = d[16];
            int ss = d[17];

            QTime t(hh, mm, ss);
            if (!t.isValid()) {
                std::cout << "INVALID HEADER TIME "
                          << hh << ":" << mm << ":" << ss << "\n";
                continue;
            }

            QDateTime pktTime(
                fromDt.date(),
                t
                );


            // std::cout << "Packet time : "
            //           << pktTime.toString(Qt::ISODate).toStdString() << "\n";

            /* TIME FILTER */
            if (pktTime < fromDt || pktTime > toDt) {
                std::cout << "Packet skipped by TIME FILTER\n";
                continue;
            }

            /* FRAME NUMBER (DESKTOP FORMULA) */
            // int frameNum = (hh * 3600) + (mm * 60) + ss + 1;

            QString skBits = binary.mid(28, 16);
            // int skId = skBits.toInt(nullptr, 2);



            QString nmsBits = binary.mid(22, 18);
            // int nmsId = nmsBits.toInt(nullptr, 2);

            /* ROW */
            QJsonObject row;

            row["data_source"] = "UPLOAD";

            // SOF
            row["sof"] = "AAAA";

            // Message Type
            row["msg_type"] = JNUM(static_cast<quint8>(raw[2]));

            // Message Length
            row["msg_length"] =
                JNUM((static_cast<quint8>(raw[3]) << 8) |
                     static_cast<quint8>(raw[4]));

            // Message Sequence
            row["msg_sequence"] =
                JNUM((static_cast<quint8>(raw[5]) << 8) |
                     static_cast<quint8>(raw[6]));

            // Stationary KAVACH ID
            row["stationary_kavach_id"] =
                JNUM((static_cast<quint8>(raw[7]) << 8) |
                     static_cast<quint8>(raw[8]));

            // NMS System ID
            row["nms_system_id"] =
                JNUM((static_cast<quint8>(raw[9]) << 8) |
                     static_cast<quint8>(raw[10]));

            // System Version
            row["system_version"] = JNUM(static_cast<quint8>(raw[11]));

            // Date (DDMMYY – raw hex)
            row["date_hex"] =
                QString("%1%2%3")
                    .arg(static_cast<quint8>(raw[12]), 2, 16, QLatin1Char('0'))
                    .arg(static_cast<quint8>(raw[13]), 2, 16, QLatin1Char('0'))
                    .arg(static_cast<quint8>(raw[14]), 2, 16, QLatin1Char('0'))
                    .toUpper();

            // Time
            row["time"] =
                QString("%1:%2:%3")
                    .arg(hh, 2, 10, QLatin1Char('0'))
                    .arg(mm, 2, 10, QLatin1Char('0'))
                    .arg(ss, 2, 10, QLatin1Char('0'));
            row["event_time"] = pktTime.toString(Qt::ISODate);

            quint8 radio = static_cast<quint8>(raw[18]);
            // row["station_active_radio"] = JNUM(radio);

            QString radioStr = "UNKNOWN";
            if (radio == 0xF1) radioStr = "RADIO_1";
            else if (radio == 0xF2) radioStr = "RADIO_2";
            else if (radio == 0xE1) radioStr = "ETHERNET_1";
            else if (radio == 0xE2) radioStr = "ETHERNET_2";

            row["station_active_radio_desc"] = radioStr;

            // // Event Time
            row["event_time"] = pktTime.toString(Qt::ISODate);



            // =================================================
            // REGULAR PACKET (1001) — HEADER (BIT-WISE, SPEC)
            // =================================================
            if (pktType == 0b1001) {

                int bit = 0;

                // 1) PKT_TYPE (4)
                int pkt_type = binary.mid(bit, 4).toInt(nullptr, 2);
                bit += 4; // already known = 1001

                // 2) PKT_LENGTH (10)
                int pkt_length = binary.mid(bit, 10).toInt(nullptr, 2);
                bit += 10;

                // 3) FRAME_NUM (17)
                int frame_num = binary.mid(bit, 17).toInt(nullptr, 2);
                bit += 17;

                // 4) SOURCE_STN_ILC_IBS_ID (16)
                int source_stn_id = binary.mid(bit, 16).toInt(nullptr, 2);
                bit += 16;

                // 5) SOURCE_STN_ILC_IBS_VERSION (3)
                int source_version = binary.mid(bit, 3).toInt(nullptr, 2);
                bit += 3;

                // 6) DEST_LOCO_ID (20)
                int dest_loco_id = binary.mid(bit, 20).toInt(nullptr, 2);
                bit += 20;

                // 7) REF_PROF_ID (4)
                int ref_profile_id = binary.mid(bit, 4).toInt(nullptr, 2);
                bit += 4;

                // 8) LAST_REF_RFID (10)
                int last_ref_rfid = binary.mid(bit, 10).toInt(nullptr, 2);
                bit += 10;

                // 9) DIST_PKT_START (15, signed)
                int dist_pkt_start = binary.mid(bit, 15).toInt(nullptr, 2);
                if (dist_pkt_start & (1 << 14)) { // sign bit
                    dist_pkt_start -= (1 << 15);
                }
                bit += 15;

                // 10) PKT_DIR (2)
                int pkt_direction = binary.mid(bit, 2).toInt(nullptr, 2);
                bit += 2;

                // 11) Padding (3)
                bit += 3;

                // ---------------- JSON OUTPUT ----------------
                row["pkt_type"]           = JNUM(pkt_type);
                row["pkt_length"]         = JNUM(pkt_length);
                row["frame_number"]       = JNUM(frame_num);
                row["source_stn_id"]      = JNUM(source_stn_id);
                row["source_version"]     = JNUM(source_version);
                row["dest_loco_id"]       = JNUM(dest_loco_id);
                row["ref_profile_id"]     = JNUM(ref_profile_id);
                row["last_ref_rfid"]      = JNUM(last_ref_rfid);
                row["dist_pkt_start_m"]   = JNUM(dist_pkt_start);
                row["pkt_direction"]      = JNUM(pkt_direction);


                // =================================================
                // SUB-PACKETS LOOP
                // =================================================
                int payloadEnd = binary.size() - 64;

                while (bit + 11 <= payloadEnd) {

                    int sub_pkt_type = binary.mid(bit, 4).toInt(nullptr, 2);
                    bit += 4;

                    int sub_pkt_len = binary.mid(bit, 7).toInt(nullptr, 2); // bytes
                    bit += 7;

                    int sub_start_bit = bit;
                    int sub_bits = sub_pkt_len * 8;

                    if (sub_pkt_type == 0b0000) {

                        // 1) FRAME_OFFSET (4)
                        int frame_offset = binary.mid(bit, 4).toInt(nullptr, 2);
                        bit += 4;

                        // 2) DEST_LOCO_SOS (4)
                        int dest_loco_sos = binary.mid(bit, 4).toInt(nullptr, 2);
                        bit += 4;

                        // 3) TRAIN_SECTION_TYPE (2)
                        int train_section_type = binary.mid(bit, 2).toInt(nullptr, 2);
                        bit += 2;

                        // 4) SIGNAL INFO (17 bits → a16..a0)
                        QString signal_bits = binary.mid(bit, 17);
                        bit += 17;

                        // Split signal info
                        int sig_stop      = signal_bits.mid(0, 1).toInt(nullptr, 2);   // a16
                        int sig_override  = signal_bits.mid(1, 1).toInt(nullptr, 2);   // a15
                        int sig_type      = signal_bits.mid(2, 6).toInt(nullptr, 2);   // a14–a9
                        int sig_line_name = signal_bits.mid(8, 4).toInt(nullptr, 2);   // a8–a5
                        int sig_line_no   = signal_bits.mid(12, 5).toInt(nullptr, 2);  // a4–a0

                        // 5) CUR_SIG_ASPECT (6)
                        int cur_sig_aspect = binary.mid(bit, 6).toInt(nullptr, 2);
                        bit += 6;

                        // 6) NEXT_SIG_ASPECT (6)
                        int next_sig_aspect = binary.mid(bit, 6).toInt(nullptr, 2);
                        bit += 6;

                        // 7) APPR_SIG_DIST (15)
                        int appr_sig_dist = binary.mid(bit, 15).toInt(nullptr, 2);
                        bit += 15;

                        // 8) AUTHORITY_TYPE (2)
                        int authority_type = binary.mid(bit, 2).toInt(nullptr, 2);
                        bit += 2;

                        // 9) AUTHORIZED_SPEED (6) → only if OS
                        int authorized_speed = -1;

                        if (authority_type == 0b01) {

                            authorized_speed = binary.mid(bit, 6).toInt(nullptr, 2);

                            if (authorized_speed >= 1 && authorized_speed <= 50)
                                row["authorized_speed_kmph"] = JNUM(authorized_speed * 5);
                            else if (authorized_speed >= 51 && authorized_speed <= 61)
                                row["authorized_speed_kmph"] = "Reserved";
                            else if (authorized_speed == 62)
                                row["authorized_speed_kmph"] = 8;
                            else if (authorized_speed == 63)
                                row["authorized_speed_kmph"] = "Unknown";
                        }

                        bit += 6;


                        // 10) MA_W_R_T_SIG (16)
                        int ma_wrt_sig = binary.mid(bit, 16).toInt(nullptr, 2);
                        bit += 16;

                        // 11) REQ_SHORTEN_MA (1)
                        int req_shorten_ma = binary.mid(bit, 1).toInt(nullptr, 2);
                        bit += 1;

                        // 12) NEW_MA (16) → only if requested
                        int new_ma = -1;
                        if (req_shorten_ma == 1) {
                            new_ma = binary.mid(bit, 16).toInt(nullptr, 2);
                            bit += 16;
                        }

                        // 13) TRN_LEN_INFO_STS (1)
                        int trn_len_info_sts = binary.mid(bit, 1).toInt(nullptr, 2);
                        bit += 1;

                        int trn_len_info_type = -1;
                        int ref_frame_num_tlm = -1;
                        int ref_offset_int_tlm = -1;

                        if (trn_len_info_sts == 1) {

                            // 14) TRN_LEN_INFO_TYPE (1)
                            trn_len_info_type = binary.mid(bit, 1).toInt(nullptr, 2);
                            bit += 1;

                            // 15) REF_FRAME_NUM_TLM (17)
                            ref_frame_num_tlm = binary.mid(bit, 17).toInt(nullptr, 2);
                            bit += 17;

                            // 16) REF_OFFSET_INT_TLM (8)
                            ref_offset_int_tlm = binary.mid(bit, 8).toInt(nullptr, 2);
                            bit += 8;
                        }

                        // 17) NEXT_STN_COMM (1)
                        int next_stn_comm = binary.mid(bit, 1).toInt(nullptr, 2);
                        bit += 1;

                        int appr_stn_ilc_ibs_id = -1;
                        if (next_stn_comm == 1) {
                            appr_stn_ilc_ibs_id = binary.mid(bit, 16).toInt(nullptr, 2);
                            bit += 16;
                        }

                        // ---------------- JSON OUTPUT ----------------
                        row["ma_frame_offset"]        = JNUM(frame_offset);
                        row["dest_loco_sos"]          = JNUM(dest_loco_sos);
                        row["train_section_type"]     = JNUM(train_section_type);

                        row["sig_stop"]               = JNUM(sig_stop);
                        row["sig_override"]           = JNUM(sig_override);
                        row["sig_type"]               = JNUM(sig_type);
                        row["sig_line_name"]          = JNUM(sig_line_name);
                        row["sig_line_no"]            = JNUM(sig_line_no);

                        row["cur_signal_aspect"]      = JNUM(cur_sig_aspect);
                        row["next_signal_aspect"]     = JNUM(next_sig_aspect);
                        row["approaching_signal_dist_m"] = JNUM(appr_sig_dist);

                        row["authority_type"]         = JNUM(authority_type);
                        if (authorized_speed >= 0)
                            row["authorized_speed"]   = JNUM(authorized_speed);

                        row["ma_distance_m"]          = JNUM(ma_wrt_sig);
                        row["req_shorten_ma"]         = JNUM(req_shorten_ma);

                        if (new_ma >= 0)
                            row["new_ma_distance_m"]  = JNUM(new_ma);

                        row["trn_len_info_sts"]       = JNUM(trn_len_info_sts);
                        if (trn_len_info_sts == 1) {
                            row["trn_len_info_type"]  = JNUM(trn_len_info_type);
                            row["ref_frame_num_tlm"]  = JNUM(ref_frame_num_tlm);
                            row["ref_offset_int_tlm"] = JNUM(ref_offset_int_tlm);
                        }

                        row["next_station_comm"]      = JNUM(next_stn_comm);
                        if (next_stn_comm == 1)
                            row["approaching_station_id"] = JNUM(appr_stn_ilc_ibs_id);

                        row["sub_pkt_type_ma"] = JNUM(sub_pkt_type);
                        row["sub_pkt_length_ma"] = JNUM(sub_pkt_len);

                    }
                    // =================================================
                    // 0001 – STATIC SPEED PROFILE (SSP)
                    // =================================================
                    else if (sub_pkt_type == 1) {

                        row["has_static_speed_profile"] = true;
                        row["sub_pkt_type_ssp"] = JNUM(sub_pkt_type);
                        row["sub_pkt_len_ssp"]  = JNUM(sub_pkt_len);

                        int lm_speed_info_cnt = binary.mid(bit, 5).toInt(nullptr, 2);
                        bit += 5;

                        row["lm_speed_info_cnt"] = JNUM(lm_speed_info_cnt);

                        QJsonArray ssp_entries;

                        for (int i = 0; i < lm_speed_info_cnt; ++i) {

                            QJsonObject ssp;

                            int dist = binary.mid(bit, 15).toInt(nullptr, 2);
                            bit += 15;

                            int speed_class = binary.mid(bit, 1).toInt(nullptr, 2);
                            bit += 1;

                            ssp["distance_m"] = JNUM(dist);
                            ssp["speed_class"] = JNUM(speed_class);

                            if (speed_class == 0) {

                                int speed_val = binary.mid(bit, 6).toInt(nullptr, 2);
                                bit += 6;

                                // SEND ONLY RAW
                                ssp["lm_static_speed_value_raw"] = JNUM(speed_val);
                            }
                            else {

                                int spA = binary.mid(bit, 6).toInt(nullptr, 2); bit += 6;
                                int spB = binary.mid(bit, 6).toInt(nullptr, 2); bit += 6;
                                int spC = binary.mid(bit, 6).toInt(nullptr, 2); bit += 6;

                                // SEND ONLY RAW
                                ssp["speed_A_raw"] = JNUM(spA);
                                ssp["speed_B_raw"] = JNUM(spB);
                                ssp["speed_C_raw"] = JNUM(spC);
                            }

                            ssp_entries.append(ssp);
                        }

                        row["static_speed_profile"] = ssp_entries;
                    }

                    // =================================================
                    // 0010 – GRADIENT PROFILE
                    // =================================================
                    else if (sub_pkt_type == 2) {

                        row["has_gradient_profile"] = true;
                        row["sub_pkt_type_grad"] = JNUM(sub_pkt_type);
                        row["sub_pkt_len_grad"]  = JNUM(sub_pkt_len);


                        // 1) LM_Grad_Info_CNT (5)
                        int grad_cnt = binary.mid(bit, 5).toInt(nullptr, 2);
                        bit += 5;

                        row["lm_grad_info_cnt"] = JNUM(grad_cnt);

                        QJsonArray gradient_entries;

                        for (int i = 0; i < grad_cnt; ++i) {

                            QJsonObject grad;

                            // 2) LM_Gradient_Distance (15)
                            int dist = binary.mid(bit, 15).toInt(nullptr, 2);
                            bit += 15;

                            // 3) LM_GDIR (1)
                            int gdir = binary.mid(bit, 1).toInt(nullptr, 2);
                            bit += 1;

                            // 4) LM_GRADIENT_VALUE (5)
                            int grad_val = binary.mid(bit, 5).toInt(nullptr, 2);
                            bit += 5;

                            grad["distance_m"] = JNUM(dist);
                            grad["direction"] = JNUM(gdir);   // 0 = downhill, 1 = uphill
                            grad["gradient_raw"] = JNUM(grad_val);

                            // Human-readable interpretation (optional but useful)
                            if (grad_val <= 30) {
                                grad["gradient_desc"] = QString("Gradient class %1").arg(grad_val);

                            } else {
                                grad["gradient_desc"] = "RESERVED";
                            }

                            gradient_entries.append(grad);
                        }

                        row["gradient_profile"] = gradient_entries;
                    }
                    // =================================================
                    // 0011 – LC GATE PROFILE
                    // =================================================
                    else if (sub_pkt_type == 3) {

                        row["has_lc_gate_profile"] = true;

                        // ===============================
                        // Annexure Header Fields
                        // ===============================
                        row["sub_pkt_type_lc"] = JNUM(sub_pkt_type);
                        row["sub_pkt_len_lc"]  = JNUM(sub_pkt_len);

                        int lc_cnt = binary.mid(bit, 5).toInt(nullptr, 2);
                        bit += 5;

                        row["lm_lc_info_cnt"] = JNUM(lc_cnt);        // Annexure
                        row["lc_gate_count"]  = JNUM(lc_cnt);        // Simplified

                        QJsonArray lc_entries;

                        for (int i = 0; i < lc_cnt; ++i) {

                            QJsonObject lc;

                            int lc_dist = binary.mid(bit, 15).toInt(nullptr, 2);
                            bit += 15;

                            int lc_id_num = binary.mid(bit, 10).toInt(nullptr, 2);
                            bit += 10;

                            int lc_suffix = binary.mid(bit, 3).toInt(nullptr, 2);
                            bit += 3;

                            int manning = binary.mid(bit, 1).toInt(nullptr, 2);
                            bit += 1;

                            int lc_class = binary.mid(bit, 3).toInt(nullptr, 2);
                            bit += 3;

                            int auto_whistle = binary.mid(bit, 1).toInt(nullptr, 2);
                            bit += 1;

                            int whistle_type = binary.mid(bit, 2).toInt(nullptr, 2);
                            bit += 2;

                            // ===================================
                            // Annexure Raw Field Names
                            // ===================================
                            lc["lm_lc_distance"]              = JNUM(lc_dist);
                            lc["lm_lc_id_numeric"]            = JNUM(lc_id_num);
                            lc["lm_lc_id_alpha_suffix"]       = JNUM(lc_suffix);
                            lc["lm_lc_manning_type"]          = JNUM(manning);
                            lc["lm_lc_class"]                 = JNUM(lc_class);
                            lc["lm_lc_autowhistling_enabled"] = JNUM(auto_whistle);
                            lc["lm_lc_auto_whistling_type"]   = JNUM(whistle_type);

                            // ===================================
                            // Simplified UI-Friendly Names
                            // ===================================
                            lc["distance_m"] = JNUM(lc_dist);
                            lc["lc_id_numeric"] = JNUM(lc_id_num);
                            lc["lc_id_suffix"] = JNUM(lc_suffix);
                            lc["manning_type"] = JNUM(manning);
                            lc["lc_class"] = JNUM(lc_class);
                            lc["auto_whistling_enabled"] = JNUM(auto_whistle);
                            lc["auto_whistling_type"] = JNUM(whistle_type);

                            lc_entries.append(lc);
                        }

                        row["lc_gate_profile"] = lc_entries;
                    }

                    // =================================================
                    // 0100 – TURNOUT SPEED PROFILE
                    // =================================================
                    else if (sub_pkt_type == 4) {

                        row["has_turnout_profile"] = true;

                        // ---- Annexure Header ----
                        row["sub_pkt_type_to"] = JNUM(sub_pkt_type);
                        row["sub_pkt_len_to"]  = JNUM(sub_pkt_len);

                        // 1) TO_CNT (2 bits)
                        int to_cnt = binary.mid(bit, 2).toInt(nullptr, 2);
                        bit += 2;

                        row["to_cnt"] = JNUM(to_cnt);

                        QJsonArray turnout_entries;

                        for (int i = 0; i < to_cnt; ++i) {

                            QJsonObject to;

                            // 2) TO_SPEED (5 bits)
                            int to_speed = binary.mid(bit, 5).toInt(nullptr, 2);
                            bit += 5;

                            // ---- RAW (Annexure names) ----
                            to["lm_to_speed_raw"] = JNUM(to_speed);

                            // ---- Simplified ----
                            to["turnout_speed_code"] = JNUM(to_speed);

                            bool restricted = (to_speed >= 1 && to_speed <= 18);

                            if (restricted) {

                                // 3) DIFF_DIST_TO (15 bits)
                                int diff_dist_to = binary.mid(bit, 15).toInt(nullptr, 2);
                                bit += 15;

                                // 4) TO_SPEED_REL_DIST (12 bits)
                                int rel_dist = binary.mid(bit, 12).toInt(nullptr, 2);
                                bit += 12;

                                // ---- RAW ----
                                to["lm_diff_dist_to"] = JNUM(diff_dist_to);
                                to["lm_to_speed_rel_dist"] = JNUM(rel_dist);

                                // ---- Simplified ----
                                to["start_distance_m"] = JNUM(diff_dist_to);
                                to["release_distance_m"] = JNUM(rel_dist);
                            }

                            turnout_entries.append(to);
                        }

                        row["turnout_speed_profile"] = turnout_entries;
                    }

                    // =================================================
                    // 0101 – TAG LINKING INFORMATION
                    // =================================================
                    else if (sub_pkt_type == 5) {

                        row["has_tag_linking"] = true;
                        row["sub_pkt_type_tag"] = JNUM(sub_pkt_type);
                        row["sub_pkt_len_tag"]  = JNUM(sub_pkt_len);

                        // 1) DIST_DUP_TAG (4)
                        int dist_dup_tag = binary.mid(bit, 4).toInt(nullptr, 2);
                        bit += 4;

                        row["dist_dup_tag_m"] = JNUM(dist_dup_tag);

                        // 2) ROUTE_RFID_CNT (6)
                        int rfid_cnt = binary.mid(bit, 6).toInt(nullptr, 2);
                        bit += 6;

                        row["route_rfid_count"] = JNUM(rfid_cnt);

                        QJsonArray rfid_list;

                        // -------------------------------------------------
                        // RFID LIST (only if count 1–62)
                        // -------------------------------------------------
                        // Bits remaining inside this subpacket
                        int sub_end_bit = sub_start_bit + sub_bits;

                        // Each RFID entry = 22 bits
                        const int RFID_ENTRY_BITS = 22;

                        // Maximum entries we can safely read
                        int max_possible_entries = (sub_end_bit - bit) / RFID_ENTRY_BITS;

                        // Final safe count
                        int safe_count = qMin(rfid_cnt, max_possible_entries);

                        for (int i = 0; i < safe_count; ++i) {

                            QJsonObject rfid;

                            int dist_next_rfid = binary.mid(bit, 11).toInt(nullptr, 2);
                            bit += 11;

                            int next_rfid_id = binary.mid(bit, 10).toInt(nullptr, 2);
                            bit += 10;

                            int dup_tag_dir = binary.mid(bit, 1).toInt(nullptr, 2);
                            bit += 1;

                            rfid["dist_next_rfid_m"] = JNUM(dist_next_rfid);
                            rfid["rfid_tag_id"]      = JNUM(next_rfid_id);
                            rfid["dup_tag_dir"]      = JNUM(dup_tag_dir);

                            rfid_list.append(rfid);
                        }


                        row["rfid_list"] = rfid_list;

                        // -------------------------------------------------
                        // 6) ABS_LOC_RESET (1)
                        // -------------------------------------------------
                        int abs_loc_reset = binary.mid(bit, 1).toInt(nullptr, 2);
                        bit += 1;

                        row["abs_loc_reset"] = JNUM(abs_loc_reset);

                        // -------------------------------------------------
                        // LOCATION RESET SECTION (only if ABS_LOC_RESET = 1)
                        // -------------------------------------------------
                        if (abs_loc_reset == 1) {

                            // 7) START_DIST_TO_LOC_RESET (15)
                            int start_dist_reset = binary.mid(bit, 15).toInt(nullptr, 2);
                            bit += 15;

                            // 8) ADJ_LOCO_DIR (2)
                            int adj_loco_dir = binary.mid(bit, 2).toInt(nullptr, 2);
                            bit += 2;

                            // 9) ABS_LOC_CORRECTION (23)
                            int abs_loc_corr = binary.mid(bit, 23).toInt(nullptr, 2);
                            bit += 23;

                            row["loc_reset_start_dist_m"] = JNUM(start_dist_reset);
                            row["adj_loco_dir"]           = JNUM(adj_loco_dir);
                            row["abs_loc_correction_m"]   = JNUM(abs_loc_corr);

                            // 10) ADJ_LINE_CNT (3)
                            int adj_line_cnt = binary.mid(bit, 3).toInt(nullptr, 2);
                            bit += 3;

                            row["adjacent_line_count"] = JNUM(adj_line_cnt);

                            QJsonArray line_tins;

                            if (adj_line_cnt <= 5) {
                                for (int i = 0; i < adj_line_cnt; ++i) {

                                    // 11) LINE_TIN (9)
                                    int line_tin = binary.mid(bit, 9).toInt(nullptr, 2);
                                    bit += 9;

                                    line_tins.append(JNUM(line_tin));
                                }
                            }

                            row["adjacent_line_tins"] = line_tins;
                        }
                    }

                    // =================================================
                    // 0110 – TRACK CONDITION DATA
                    // =================================================
                    else if (sub_pkt_type == 6) {

                        row["has_track_condition"] = true;
                        row["sub_pkt_type_track"] = JNUM(sub_pkt_type);
                        row["sub_pkt_len_track"]  = JNUM(sub_pkt_len);

                        // 1) TRACKCOND_CNT (4)
                        int trackcond_cnt = binary.mid(bit, 4).toInt(nullptr, 2);
                        bit += 4;

                        row["track_condition_count"] = JNUM(trackcond_cnt);

                        QJsonArray track_conditions;

                        // -------------------------------------------------
                        // Track Condition Entries
                        // -------------------------------------------------
                        for (int i = 0; i < trackcond_cnt; ++i) {

                            QJsonObject tc;

                            // 2) TRACKCOND_TYPE (4)
                            int trackcond_type = binary.mid(bit, 4).toInt(nullptr, 2);
                            bit += 4;

                            // 3) START_DIST_TRACKCOND (15)
                            int start_dist = binary.mid(bit, 15).toInt(nullptr, 2);
                            bit += 15;

                            // 4) LENGTH_TRACKCOND (15)
                            int length_dist = binary.mid(bit, 15).toInt(nullptr, 2);
                            bit += 15;

                            tc["track_condition_type_raw"] = JNUM(trackcond_type);

                            if (trackcond_type <= 9)
                                tc["track_condition_type"] = JNUM(trackcond_type);
                            else
                                tc["track_condition_type"] = "Reserved";

                            tc["start_dist_m"]         = JNUM(start_dist);
                            tc["length_m"]             = JNUM(length_dist);

                            track_conditions.append(tc);
                        }

                        row["track_conditions"] = track_conditions;
                    }
                    // =================================================
                    // 0111 – TEMPORARY SPEED RESTRICTION (TSR) PROFILE
                    // =================================================
                    else if (sub_pkt_type == 7) {

                        row["has_tsr_profile"] = true;
                        row["sub_pkt_type_tsr"] = JNUM(sub_pkt_type);
                        row["sub_pkt_len_tsr"]  = JNUM(sub_pkt_len);

                        // 1) TSR_STATUS (2)
                        int tsr_status = binary.mid(bit, 2).toInt(nullptr, 2);
                        bit += 2;

                        row["tsr_status"] = JNUM(tsr_status);

                        // If no TSR applicable or no latest TSR info
                        if (tsr_status != 2) {
                            bit = sub_start_bit + sub_bits;
                            continue;
                        }


                        // 2) TSR_INFO_CNT (5)
                        int tsr_cnt = binary.mid(bit, 5).toInt(nullptr, 2);
                        bit += 5;

                        row["tsr_info_cnt"] = JNUM(tsr_cnt);

                        QJsonArray tsr_list;


                        // -------------------------------------------------
                        // TSR Entries
                        // -------------------------------------------------
                        for (int i = 0; i < tsr_cnt; ++i) {

                            QJsonObject tsr;

                            // 3) TSR_ID (8)
                            int tsr_id = binary.mid(bit, 8).toInt(nullptr, 2);
                            bit += 8;

                            // 4) TSR_DISTANCE (15)
                            int tsr_distance = binary.mid(bit, 15).toInt(nullptr, 2);
                            bit += 15;

                            // 5) TSR_LENGTH (15)
                            int tsr_length = binary.mid(bit, 15).toInt(nullptr, 2);
                            bit += 15;

                            // 6) TSR_CLASS (1)
                            int tsr_class = binary.mid(bit, 1).toInt(nullptr, 2);
                            bit += 1;

                            tsr["tsr_id"]           = JNUM(tsr_id);
                            tsr["tsr_distance_m"]   = JNUM(tsr_distance);
                            tsr["tsr_length_m"]     = JNUM(tsr_length);
                            tsr["tsr_class"]        = JNUM(tsr_class);

                            // -------------------------------------------------
                            // Universal Speed
                            // -------------------------------------------------
                            if (tsr_class == 0) {

                                int tsr_univ_speed = binary.mid(bit, 6).toInt(nullptr, 2);
                                bit += 6;

                                if (tsr_univ_speed == 0)
                                    tsr["tsr_universal_speed_kmph"] = "Dead Stop";
                                else if (tsr_univ_speed >= 1 && tsr_univ_speed <= 40)
                                    tsr["tsr_universal_speed_kmph"] = JNUM(tsr_univ_speed * 5);
                                else if (tsr_univ_speed == 62)
                                    tsr["tsr_universal_speed_kmph"] = 8;
                                else if (tsr_univ_speed == 63)
                                    tsr["tsr_universal_speed_kmph"] = "Unknown";
                                else
                                    tsr["tsr_universal_speed_kmph"] = "Reserved";

                            }
                            // -------------------------------------------------
                            // Classified Speed (A / B / C)
                            // -------------------------------------------------
                            else {

                                int speed_a = binary.mid(bit, 6).toInt(nullptr, 2);
                                bit += 6;

                                int speed_b = binary.mid(bit, 6).toInt(nullptr, 2);
                                bit += 6;

                                int speed_c = binary.mid(bit, 6).toInt(nullptr, 2);
                                bit += 6;

                                tsr["tsr_classA_speed"] = JNUM(speed_a);
                                tsr["tsr_classB_speed"] = JNUM(speed_b);
                                tsr["tsr_classC_speed"] = JNUM(speed_c);
                            }

                            int tsr_whistle = binary.mid(bit, 2).toInt(nullptr, 2);
                            bit += 2;

                            tsr["tsr_whistle_raw"] = JNUM(tsr_whistle);

                            switch(tsr_whistle) {
                            case 0: tsr["tsr_whistle"] = "No Whistle"; break;
                            case 1: tsr["tsr_whistle"] = "Whistle Blow"; break;
                            default: tsr["tsr_whistle"] = "Spare"; break;
                            }


                            tsr_list.append(tsr);
                        }

                        row["tsr_list"] = tsr_list;

                    }

                    int expected_end = sub_start_bit + sub_bits;
                    if (bit < expected_end) {
                        bit = expected_end;
                    }

                }


            }

            // =================================================
            // ACCESS FIELDS(1011)
            // =================================================


            else if (pktType == 0b1011) {
                std::cout << " ACCESS PACKET BLOCK ENTERED " << std::endl;


                int bit = 0;

                // 1) PKT_TYPE (4)
                int pkt_type = binary.mid(bit, 4).toInt(nullptr, 2);
                bit += 4;

                // 2) PKT_LENGTH (7)
                int pkt_len = binary.mid(bit, 7).toInt(nullptr, 2);
                bit += 7;

                // 3) FRAME_NUM (17)
                int frame_num = binary.mid(bit, 17).toInt(nullptr, 2);
                bit += 17;

                // 4) SOURCE_STN_ILC_IBS_ID (16)
                int src_id = binary.mid(bit, 16).toInt(nullptr, 2);
                bit += 16;

                // 5) SOURCE_STN_ILC_IBS_VERSION (3)
                int src_ver = binary.mid(bit, 3).toInt(nullptr, 2);
                bit += 3;

                // 6) STN_ILC_IBS_LOC (23)
                int stn_loc = binary.mid(bit, 23).toInt(nullptr, 2);
                bit += 23;

                // 7) DEST_LOCO_ID (20)
                int loco_id = binary.mid(bit, 20).toInt(nullptr, 2);
                bit += 20;

                // 8) Allotted_UpLink_Freq (12)
                int uplink_freq = binary.mid(bit, 12).toInt(nullptr, 2);
                bit += 12;

                // 9) Allotted_DownLink_Freq (12)
                int downlink_freq = binary.mid(bit, 12).toInt(nullptr, 2);
                bit += 12;

                // 10) Allotted_TDMA_Timeslot (7)
                int tdma_slot = binary.mid(bit, 7).toInt(nullptr, 2);
                bit += 7;

                // 11) STN_RND_NUM_RS (16)
                int rnd_rs = binary.mid(bit, 16).toInt(nullptr, 2);
                bit += 16;

                // 12) STN_TDMA (7)
                int stn_tdma = binary.mid(bit, 7).toInt(nullptr, 2);
                bit += 7;

                // 13) MAC_CODE (32)
                quint32 mac_code = binary.mid(bit, 32).toUInt(nullptr, 2);
                bit += 32;

                // 14) PKT_CRC (32)
                quint32 pkt_crc = binary.mid(bit, 32).toUInt(nullptr, 2);
                bit += 32;

                // ================= JSON =================

                row["pkt_type"] = JNUM(pkt_type);
                row["pkt_length"] = JNUM(pkt_len);
                row["frame_number"] = JNUM(frame_num);

                row["source_stn_id"] = JNUM(src_id);
                row["source_version"] = JNUM(src_ver);
                row["stn_location_m"] = JNUM(stn_loc);

                row["dest_loco_id"] = JNUM(loco_id);

                row["uplink_freq_channel"] = JNUM(uplink_freq);
                row["downlink_freq_channel"] = JNUM(downlink_freq);

                row["tdma_timeslot"] = JNUM(tdma_slot);
                row["stn_random_rs"] = JNUM(rnd_rs);
                row["stn_tdma"] = JNUM(stn_tdma);

                row["mac_code"] = JNUM(mac_code);
                row["pkt_crc"] = JNUM(pkt_crc);
            }

            // =================================================
            // ADDITIONAL EMERGENCY PACKET (1100) — ANNEXURE-C ALIGNED
            // =================================================
            else if (pktType == 0b1100) {

                std::cout << " EMERGENCY PACKET BLOCK ENTERED \n";

                int bit = 0;

                // 1) PKT_TYPE (4)
                int pkt_type = binary.mid(bit, 4).toInt(nullptr, 2);
                bit += 4;

                // 2) PKT_LENGTH (7)
                int pkt_length = binary.mid(bit, 7).toInt(nullptr, 2);
                bit += 7;

                // 3) FRAME_NUM (17)
                int frame_number = binary.mid(bit, 17).toInt(nullptr, 2);
                bit += 17;

                // 4) SOURCE_STN_ILC_IBS_ID (16)
                int source_stn_id = binary.mid(bit, 16).toInt(nullptr, 2);
                bit += 16;

                // 5) SOURCE_STN_ILC_IBS_VERSION (3)
                int source_version = binary.mid(bit, 3).toInt(nullptr, 2);
                bit += 3;

                // 6) STN_ILC_IBS_LOC (23)
                int stn_location = binary.mid(bit, 23).toInt(nullptr, 2);
                bit += 23;

                // 7) GEN_SOS_CALL (1)
                int gen_sos_call = binary.mid(bit, 1).toInt(nullptr, 2);
                bit += 1;

                // 8) Padding (1 bit)
                bit += 1;

                // 9) PKT_CRC (32)
                quint32 pkt_crc = binary.mid(bit, 32).toUInt(nullptr, 2);
                bit += 32;

                // ================= JSON OUTPUT =================

                row["pkt_type"]         = JNUM(pkt_type);
                row["pkt_length"]       = JNUM(pkt_length);
                row["frame_number"]     = JNUM(frame_number);

                row["source_stn_id"]    = JNUM(source_stn_id);
                row["source_version"]   = JNUM(source_version);
                row["stn_location_m"]   = JNUM(stn_location);

                row["gen_sos_call"]     = JNUM(gen_sos_call);

                row["pkt_crc"]          = JNUM(pkt_crc);
            }

            rows.append(row);

        }


    return {{"success", true}, {"data", rows}};
}


