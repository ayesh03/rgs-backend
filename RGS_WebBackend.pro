QT += core sql httpserver gui network
CONFIG += console c++17

INCLUDEPATH += $$PWD/config


SOURCES += \
    backend_database.cpp \
    backend_fault_summary.cpp \
    backend_gprs_fault.cpp \
    backend_interlocking.cpp \
    backend_loco_fault.cpp \
    backend_loco_movement.cpp \
    backend_rfcom_fault.cpp \
    backend_stationary_health.cpp \
    backend_stationary_kavach.cpp \
    config/track_profile_config.cpp \
    graph_backend.cpp \
    lvk_fault_packet.cpp \
    lvk_fault_parser.cpp \
    lvk_pos_info_parser.cpp \
    main.cpp \
    backend_db.cpp \
    parameter_report_backend.cpp \
    track_profile_graph_backend.cpp \
    track_profile_report_backend.cpp \
    config/interlocking_relays_config.cpp \
    config/stations_config.cpp

HEADERS += \
    backend_database.h \
    backend_db.h \
    backend_fault_summary.h \
    backend_gprs_fault.h \
    backend_interlocking.h \
    backend_loco_fault.h \
    backend_loco_movement.h \
    backend_rfcom_fault.h \
    backend_stationary_health.h \
    backend_stationary_kavach.h \
    config/track_profile_config.h \
    dbconfig.h \
    graph_backend.h \
    lvk_fault_packet.h \
    lvk_fault_parser.h \
    lvk_pos_info_packet.h \
    lvk_pos_info_parser.h \
    parameter_report_backend.h \
    track_profile_graph_backend.h \
    track_profile_report_backend.h \
    config/stations_config.h \
    config/interlocking_relays_config.h

DEFINES += QT_MESSAGELOGCONTEXT

DISTFILES += \
    Dockerfile




