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
    bool matchesParsedFilter(const QString &value, const ParsedFilter &pf,
                             bool exactMatch = false) const;
    static int levelIndex(const QString &level);
};

#endif // LOGFILTER_H
