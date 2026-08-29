#ifndef ILOGFILTER_H
#define ILOGFILTER_H

#include <QString>

#include "logentry.h"
#include "logquery.h"

// Everything a log line is checked against. Built once per filter change by
// LogFilterController; passesFilter() only reads it, so one instance serves
// every worker of the parallel filter.
struct FilterCriteria {
    // The filter box, parsed. An empty query places no constraint.
    LogQuery query;
    QString startTime;
    QString endTime;
    // Level floor as an ordinal: -1 = no filter; 0=V 1=D 2=I 3=W 4=E 5=A
    int minLevelIndex = -1;

    /**
     * True when at least one constraint is set.
     *
     * When nothing is active the caller can skip filtering entirely and share
     * the source vector, instead of copying every entry through a predicate
     * that always returns true.
     */
    bool isActive() const {
        return !query.isEmpty()
               || !startTime.isEmpty() || !endTime.isEmpty()
               || minLevelIndex > 0;
    }
};

class ILogFilter
{
public:
    virtual ~ILogFilter() = default;

    // Check if a log entry passes the filter
    virtual bool passesFilter(const LogEntry &entry, const FilterCriteria &criteria) const = 0;
};

#endif // ILOGFILTER_H
