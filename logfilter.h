#ifndef LOGFILTER_H
#define LOGFILTER_H

#include "ilogfilter.h"
#include <QRegularExpression>

class LogFilter : public ILogFilter
{
public:
    LogFilter() = default;
    ~LogFilter() override = default;
    
    bool passesFilter(const LogEntry &entry, const FilterCriteria &criteria) const override;
    
private:
    bool matchesStringFilter(const QString &value, const QString &filter, 
                            FilterOperator op, bool exactMatch = false) const;
};

#endif // LOGFILTER_H
