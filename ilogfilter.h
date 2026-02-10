#ifndef ILOGFILTER_H
#define ILOGFILTER_H

#include <QString>
#include "ilogconverter.h"

enum class FilterOperator {
    OR,   // Any keyword matches (||)
    AND   // All keywords must match (&&)
};

struct FilterCriteria {
    QString messageFilter;
    QString startTime;
    QString endTime;
    QString tagFilter;
    FilterOperator tagOperator = FilterOperator::OR;
    QString packageFilter;
    FilterOperator packageOperator = FilterOperator::OR;
    QString pidFilter;
    FilterOperator pidOperator = FilterOperator::OR;
    QString tidFilter;
    FilterOperator tidOperator = FilterOperator::OR;
    QString minLevel;
};

class ILogFilter
{
public:
    virtual ~ILogFilter() = default;
    
    // Check if a log entry passes the filter
    virtual bool passesFilter(const LogEntry &entry, const FilterCriteria &criteria) const = 0;
};

#endif // ILOGFILTER_H
