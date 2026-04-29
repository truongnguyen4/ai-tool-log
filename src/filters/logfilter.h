#ifndef LOGFILTER_H
#define LOGFILTER_H

#include "ilogfilter.h"

class LogFilter : public ILogFilter
{
public:
    LogFilter() = default;
    ~LogFilter() override = default;

    bool passesFilter(const LogEntry &entry, const FilterCriteria &criteria) const override;

    // Maps a log-level letter ("V"/"D"/"I"/"W"/"E"/"A") to a 0..5 ordinal,
    // or -1 if unrecognized. Public so LogFilterController can reuse it.
    static int levelIndex(const QString &level);
};

#endif // LOGFILTER_H
