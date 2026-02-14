#include <QCoreApplication>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QTcpServer>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHttpHeaders>
#include <QJsonArray>


// Backend modules
#include "backend_database.h"
#include "backend_loco_fault.h"
#include "backend_db.h"
#include "backend_loco_movement.h"
#include "backend_interlocking.h"
#include "graph_backend.h"
#include "track_profile_graph_backend.h"
#include "track_profile_report_backend.h"
#include "backend_stationary_kavach.h"
#include "backend_stationary_health.h"

#undef QT_NO_DEBUG_OUTPUT

        // =====================================================
        // Helper to create CORS headers (Qt 6.8 compatible)
        // =====================================================
        QHttpHeaders createCorsHeaders()
{
    QHttpHeaders headers;
    headers.append("Access-Control-Allow-Origin", "*");
    headers.append("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    headers.append("Access-Control-Allow-Headers", "Content-Type, Authorization");
    headers.append("Access-Control-Allow-Headers", "*");
    return headers;
}

// =====================================================
// Helper to create JSON response with CORS
// =====================================================
QHttpServerResponse corsResponse(
    const QJsonObject &body,
    QHttpServerResponse::StatusCode status =
    QHttpServerResponse::StatusCode::Ok)
{
    QHttpServerResponse res(body, status);
    res.setHeaders(createCorsHeaders());
    return res;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QHttpServer httpServer;
    QTcpServer tcpServer;



    // =====================================================
    // OPTIONS handler (CORS preflight)
    // =====================================================
    httpServer.route("/*", QHttpServerRequest::Method::Options, []() {
        QHttpServerResponse res(QHttpServerResponse::StatusCode::NoContent);
        res.setHeaders(createCorsHeaders());
        return res;
    });

    // =====================================================
    // HEALTH
    // =====================================================
    httpServer.route("/health", []() {
        return corsResponse({
            {"status", "RGS Backend Running"},
            {"version", "1.0.0"}
        });
    });



    // =====================================================
    // USER / AUTH
    // =====================================================



    // ================= CORS PREFLIGHT FOR LOGIN =================
    httpServer.route(
        "/api/auth/login",
        QHttpServerRequest::Method::Options,
        []() {
            QHttpServerResponse res(QHttpServerResponse::StatusCode::NoContent);

            QHttpHeaders headers;
            headers.append("Access-Control-Allow-Origin", "*");
            headers.append("Access-Control-Allow-Methods", "POST, OPTIONS");
            headers.append("Access-Control-Allow-Headers", "Content-Type");

            res.setHeaders(headers);
            return res;
        }
        );

    httpServer.route(
        "/api/auth/login",
        QHttpServerRequest::Method::Post,
        [](const QHttpServerRequest &req) {

            QJsonParseError err;
            QJsonDocument doc =
                QJsonDocument::fromJson(req.body(), &err);

            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                return corsResponse({
                    {"success", false},
                    {"error", "Invalid JSON body"}
                });
            }

            QJsonObject body = doc.object();
            QString username = body.value("username").toString();
            QString password = body.value("password").toString();

            if (username.isEmpty() || password.isEmpty()) {
                return corsResponse({
                    {"success", false},
                    {"error", "Username or password missing"}
                });
            }

            return corsResponse(
                BackendDatabase::loginUser(username, password)
                );
        }
        );


    httpServer.route(
        "/api/loco-movement/by-date",
        QHttpServerRequest::Method::Options,
        []() {
            QHttpServerResponse res(QHttpServerResponse::StatusCode::NoContent);
            res.setHeaders(createCorsHeaders());
            return res;
        }
        );


    // --------------------------------------------
    // FETCH LOCO MOVEMENT BY DATE RANGE
    // --------------------------------------------
    httpServer.route(
        "/api/loco-movement/by-date",
        QHttpServerRequest::Method::Post,
        [](const QHttpServerRequest &req) {

            QUrlQuery query(req.url().query());

            QString fromDate = query.queryItemValue("from");
            QString toDate   = query.queryItemValue("to");

            QByteArray fileData = req.body();  // uploaded file

            return corsResponse(
                BackendLocoMovement::fetchByDateRange(fromDate, toDate, fileData)
                );
        }
        );



    // FETCH LATEST
    httpServer.route("/api/loco-movement/latest", [](const QHttpServerRequest &req) {
        QUrlQuery query(req.url().query());

        QString fromDate = query.queryItemValue("from");
        QString toDate   = query.queryItemValue("to");

        QString fromStr = QUrl::fromPercentEncoding(fromDate.toUtf8());
        QString toStr   = QUrl::fromPercentEncoding(toDate.toUtf8());

        // Normalize ISO format
        fromStr.replace("T", " ");
        toStr.replace("T", " ");
        // ===== Robust date parser =====
        auto parseDate = [](const QString &s) -> QDateTime {
            QDateTime dt;

            dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
            if (dt.isValid()) return dt;

            dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm");
            if (dt.isValid()) return dt;

            dt = QDateTime::fromString(s, Qt::ISODate);
            if (dt.isValid()) return dt;

            return QDateTime();
        };

        QDateTime fromDt = parseDate(fromStr);
        QDateTime toDt   = parseDate(toStr);

        qInfo().noquote()
            << "FETCH LOCO MOVEMENT DATA"
            << "FROM:" << fromDt
            << "TO:"   << toDt;

        // fprintf(stderr,
        //         "[%s] FETCH LOCO MOVEMENT | FROM=%s | TO=%s\n",
        //         fromDate.toUtf8().constData(),
        //         toDate.toUtf8().constData());
        // fflush(stderr);
        return corsResponse(
            BackendLocoMovement::fetchLatest(100)
            );
    });


    // =====================================================
    // LOCO WISE
    // =====================================================
    httpServer.route("/api/init/loco-wise", []() {
        return corsResponse(BackendDatabase::createLocoWiseTable());
    });

    httpServer.route("/api/loco-wise-report", []() {
        return corsResponse(BackendDatabase::getLocoWiseReport());
    });

    httpServer.route("/api/loco-wise/insert-sample", []() {
        return corsResponse(BackendDatabase::insertLocoWiseSample());
    });


    // --------------------------------------------
    // FETCH LOCO FAULTS BY DATE RANGE
    // --------------------------------------------
    httpServer.route(
        "/api/loco-faults/by-date",
        [](const QHttpServerRequest &req) {

            QUrlQuery query(req.url().query());

            QString fromDate = query.queryItemValue("from");
            QString toDate   = query.queryItemValue("to");
            QString logDir   = query.queryItemValue("logDir");

            return corsResponse(
                BackendLocoFault::fetchByDateRange(fromDate, toDate, logDir)
                );
        }
        );

    // =====================================================
    // INTERLOCKING : FETCH STATIONS BY DATE RANGE
    // =====================================================
    httpServer.route(
        "/api/interlocking/stations",
        [](const QHttpServerRequest &req) {

            QUrlQuery query(req.url().query());

            QString from   = query.queryItemValue("from");
            QString to     = query.queryItemValue("to");
            QString logDir = query.queryItemValue("logDir");

            return corsResponse(
                BackendInterlocking().getStationsForDateRange(
                    logDir,
                    from,
                    to
                    )
                );
        }
        );


    // =====================================================
    // INTERLOCKING : GENERATE REPORT (DATE RANGE)
    // =====================================================
    httpServer.route(
        "/api/interlocking/report",
        [](const QHttpServerRequest &req) {

            QUrlQuery query(req.url().query());

            QString from        = query.queryItemValue("from");
            QString to          = query.queryItemValue("to");
            QString logDir      = query.queryItemValue("logDir");
            QString stationCode = query.queryItemValue("station");
            int page            = query.queryItemValue("page").toInt();

            if (page <= 0) page = 1;

            return corsResponse(
                BackendInterlocking().generateReportByDateRange(
                    logDir,
                    from,
                    to,
                    stationCode,
                    page
                    )
                );
        }
        );

    // =====================================================
    // GRAPH META (Locos, Dates, Directions, Graph Types)
    // =====================================================
    httpServer.route("/api/graph/meta", [](const QHttpServerRequest &req) {

        QUrlQuery query(req.url().query());

        QString logDir = query.queryItemValue("logDir");
        QString from   = query.queryItemValue("from");
        QString to     = query.queryItemValue("to");

        return corsResponse(
            GraphBackend::getGraphMeta(
                QUrl::fromPercentEncoding(logDir.toUtf8()),
                from,
                to
                )
            );
    });


    // =====================================================
    // GRAPH DATA (Location/Time vs Speed/Mode)
    // =====================================================
    httpServer.route(
        "/api/graph/data",
        QHttpServerRequest::Method::Get,
        [](const QHttpServerRequest &req) {

            QUrlQuery query(req.url().query());

            QString locoId     = query.queryItemValue("locoId");
            QString fromDate   = query.queryItemValue("from");
            QString toDate     = query.queryItemValue("to");
            QString direction  = query.queryItemValue("direction");
            QString graphType  = query.queryItemValue("graphType");
            QString logDir     = query.queryItemValue("logDir");

            if (locoId.isEmpty() || fromDate.isEmpty() || toDate.isEmpty() ||
                direction.isEmpty() || graphType.isEmpty() || logDir.isEmpty())
            {
                return corsResponse({
                    {"success", false},
                    {"error", "Missing required query parameters"}
                });
            }

            return corsResponse(
                GraphBackend::getGraphData(
                    locoId,
                    fromDate,
                    toDate,
                    direction,
                    QString(), // profileId (reserved)
                    graphType,
                    QUrl::fromPercentEncoding(logDir.toUtf8())
                    )
                );
        }
        );
    // =====================================================
    // TRACK PROFILE GRAPH : META
    // =====================================================
    httpServer.route(
        "/api/track-profile/meta",
        QHttpServerRequest::Method::Get,
        [](const QHttpServerRequest &req) {

            QUrlQuery query(req.url().query());

            QString logDir = query.queryItemValue("logDir");
            QString from   = query.queryItemValue("from");
            QString to     = query.queryItemValue("to");

            return corsResponse(
                TrackProfileGraphBackend::getMeta(
                    QUrl::fromPercentEncoding(logDir.toUtf8()),
                    from,
                    to
                    )
                );
        }
        );


    // =====================================================
    // TRACK PROFILE GRAPH : DATA (Speed + Gradient)
    // =====================================================
    httpServer.route(
        "/api/track-profile/graph",
        QHttpServerRequest::Method::Get,
        [](const QHttpServerRequest &req) {

            QUrlQuery query(req.url().query());

            QString locoId    = query.queryItemValue("locoId");
            QString station   = query.queryItemValue("station");
            QString direction = query.queryItemValue("direction");
            QString profileId = query.queryItemValue("profileId");
            QString fromDate  = query.queryItemValue("from");
            QString toDate    = query.queryItemValue("to");
            QString logDir    = query.queryItemValue("logDir");

            if (locoId.isEmpty() || station.isEmpty() || direction.isEmpty() ||
                profileId.isEmpty() || fromDate.isEmpty() || toDate.isEmpty() ||
                logDir.isEmpty())
            {
                return corsResponse({
                    {"success", false},
                    {"error", "Missing required query parameters"}
                });
            }

            return corsResponse(
                TrackProfileGraphBackend::getGraphData(
                    locoId,
                    station,
                    direction,
                    profileId,
                    fromDate,
                    toDate,
                    QUrl::fromPercentEncoding(logDir.toUtf8())
                    )
                );
        }
        );

    // =====================================================
    // TRACK PROFILE REPORT : DATA (TABLE)
    // =====================================================
    httpServer.route(
        "/api/track-profile/stations",
        QHttpServerRequest::Method::Get,
        []() {
            return corsResponse(
                TrackProfileReportBackend::getAllStations()
                );
        }
        );

    httpServer.route(
        "/api/track-profile/report",
        QHttpServerRequest::Method::Get,
        [](const QHttpServerRequest &req) {

            QUrlQuery query(req.url().query());

            QString from   = query.queryItemValue("from");
            QString to     = query.queryItemValue("to");
            QString logDir = query.queryItemValue("logDir");


            QString stationsStr = query.queryItemValue("stations");
            QStringList stations;

            if (!stationsStr.isEmpty())
                stations = stationsStr.split(",", Qt::SkipEmptyParts);

            if (from.isEmpty() || to.isEmpty() || logDir.isEmpty())
            {
                return corsResponse({
                    {"success", false},
                    {"error", "Missing required query parameters"}
                });
            }

            return corsResponse(
                TrackProfileReportBackend::getReport(
                    from,
                    to,
                    QUrl::fromPercentEncoding(logDir.toUtf8()),
                    stations
                    )
                );
        }
        );
    httpServer.route(
        "/api/stationary/regular/by-date",
        QHttpServerRequest::Method::Options,
        []() {
            QHttpServerResponse res(QHttpServerResponse::StatusCode::NoContent);
            res.setHeaders(createCorsHeaders());
            return res;
        }
        );

    // --------------------------------------------
    // STATIONARY KAVACH – REGULAR
    // --------------------------------------------
    httpServer.route(
        "/api/stationary/regular/by-date",
        QHttpServerRequest::Method::Post,
        [](const QHttpServerRequest& req) {

            QUrlQuery q(req.url().query());

            return corsResponse(
                BackendStationaryKavach::fetchRegular(
                    q.queryItemValue("from"),
                    q.queryItemValue("to"),
                    req.body()   // <-- FILE DATA
                    )
                );
        }
        );
    httpServer.route(
        "/api/stationary/access/by-date",
        QHttpServerRequest::Method::Options,
        []() {
            QHttpServerResponse res(QHttpServerResponse::StatusCode::NoContent);
            res.setHeaders(createCorsHeaders());
            return res;
        }
        );

    httpServer.route(
        "/api/stationary/access/by-date",
        QHttpServerRequest::Method::Post,
        [](const QHttpServerRequest& req) {

            QUrlQuery q(req.url().query());

            return corsResponse(
                BackendStationaryKavach::fetchAccess(
                    q.queryItemValue("from"),
                    q.queryItemValue("to"),
                    req.body()   // <-- FILE DATA
                    )
                );
        }
        );

    httpServer.route(
        "/api/stationary/emergency/by-date",
        QHttpServerRequest::Method::Options,
        []() {
            QHttpServerResponse res(QHttpServerResponse::StatusCode::NoContent);
            res.setHeaders(createCorsHeaders());
            return res;
        }
        );


    httpServer.route(
        "/api/stationary/emergency/by-date",
        QHttpServerRequest::Method::Post,
        [](const QHttpServerRequest& req) {

            QUrlQuery q(req.url().query());

            return corsResponse(
                BackendStationaryKavach::fetchEmergency(
                    q.queryItemValue("from"),
                    q.queryItemValue("to"),
                    req.body()   // <-- FILE DATA
                    )
                );
        }
        );





    // =====================================================
    // SERVER START
    // =====================================================
    quint16 port = qEnvironmentVariableIntValue("PORT");
    if (port == 0) port = 8080;

    if (!tcpServer.listen(QHostAddress::Any, port)) {
        qFatal("Failed to listen on port");
    }


    httpServer.bind(&tcpServer);
    qInfo() << " Backend running on http://localhost:8080";

    return app.exec();
}
