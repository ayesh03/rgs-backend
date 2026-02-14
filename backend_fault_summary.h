#ifndef BACKEND_FAULT_SUMMARY_H
#define BACKEND_FAULT_SUMMARY_H

#include <QJsonObject>

class BackendFaultSummary
{
public:
    static QJsonObject getSummary();
};

#endif // BACKEND_FAULT_SUMMARY_H
