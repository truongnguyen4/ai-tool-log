#include "logfilter.h"

bool LogFilter::passesFilter(const LogEntry &entry, const FilterCriteria &criteria) const
{
    // Level and time are plain comparisons, so they run first and spare the
    // lines they reject the text search.
    //
    // Level index 0 is Verbose, i.e. "no floor": at that setting nothing is
    // dropped, not even a line whose level letter we failed to recognise.
    if (criteria.minLevelIndex > 0
        && levelIndex(entry.level) < criteria.minLevelIndex)
        return false;

    if (!criteria.startTime.isEmpty() && entry.time < criteria.startTime)
        return false;
    if (!criteria.endTime.isEmpty()   && entry.time > criteria.endTime)
        return false;

    return criteria.query.matches(entry);
}

// ---------------------------------------------------------------------------
// O(1) level ordinal – avoids creating QStringList{"V","D",...} per entry
// ---------------------------------------------------------------------------
int LogFilter::levelIndex(const QString &level)
{
    if (level.isEmpty()) return -1;
    switch (level.at(0).toLatin1()) {
        case 'V': return 0;
        case 'D': return 1;
        case 'I': return 2;
        case 'W': return 3;
        case 'E': return 4;
        case 'A': return 5;
        // logcat prints Fatal, which Android ranks with Assert.
        case 'F': return 5;
        default:  return -1;
    }
}

