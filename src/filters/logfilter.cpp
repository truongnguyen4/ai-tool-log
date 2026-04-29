#include "logfilter.h"
#include "filterengine.h"

bool LogFilter::passesFilter(const LogEntry &entry, const FilterCriteria &criteria) const
{
    // Keyword regex: matched against tag, message, or package.
    if (!criteria.keywordFilter.isEmpty()
        && !FilterEngine::matchesRegex({entry.tag, entry.message, entry.package},
                                       criteria.keywordRegex))
        return false;

    // String filters – use pre-parsed form (zero allocations per entry).
    if (!FilterEngine::matchesLogic(entry.message, criteria.parsedMessage)) return false;
    if (!FilterEngine::matchesLogic(entry.tag,     criteria.parsedTag))     return false;
    if (!FilterEngine::matchesLogic(entry.package, criteria.parsedPackage)) return false;
    if (!FilterEngine::matchesLogic(entry.pid,     criteria.parsedPid, /*exact=*/true)) return false;
    if (!FilterEngine::matchesLogic(entry.tid,     criteria.parsedTid, /*exact=*/true)) return false;

    // Time range
    if (!criteria.startTime.isEmpty() && entry.time < criteria.startTime)
        return false;
    if (!criteria.endTime.isEmpty()   && entry.time > criteria.endTime)
        return false;

    // Level filter – O(1) switch, no QStringList allocation
    if (criteria.minLevelIndex >= 0
        && levelIndex(entry.level) < criteria.minLevelIndex)
        return false;

    return true;
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
        default:  return -1;
    }
}

