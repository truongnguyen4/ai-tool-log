#include "logfilter.h"

bool LogFilter::passesFilter(const LogEntry &entry, const FilterCriteria &criteria) const
{
    // Keyword regex: matched against tag, message, or package
    if (!criteria.keywordFilter.isEmpty() && criteria.keywordRegex.isValid()) {
        if (!criteria.keywordRegex.match(entry.tag).hasMatch()
         && !criteria.keywordRegex.match(entry.message).hasMatch()
         && !criteria.keywordRegex.match(entry.package).hasMatch())
            return false;
    }

    // String filters – use pre-parsed form (zero allocations per entry)
    if (criteria.parsedMessage.active
            && !matchesParsedFilter(entry.message, criteria.parsedMessage))
        return false;

    if (criteria.parsedTag.active
            && !matchesParsedFilter(entry.tag, criteria.parsedTag))
        return false;

    if (criteria.parsedPackage.active
            && !matchesParsedFilter(entry.package, criteria.parsedPackage))
        return false;

    if (criteria.parsedPid.active
            && !matchesParsedFilter(entry.pid, criteria.parsedPid, /*exactMatch=*/true))
        return false;

    if (criteria.parsedTid.active
            && !matchesParsedFilter(entry.tid, criteria.parsedTid, /*exactMatch=*/true))
        return false;

    // Time range
    if (!criteria.startTime.isEmpty() && entry.time < criteria.startTime)
        return false;
    if (!criteria.endTime.isEmpty() && entry.time > criteria.endTime)
        return false;

    // Level filter – O(1) switch, no QStringList allocation
    if (criteria.minLevelIndex >= 0) {
        if (levelIndex(entry.level) < criteria.minLevelIndex)
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Fast path: pre-parsed filter tokens, no splitting or trimming
// ---------------------------------------------------------------------------
bool LogFilter::matchesParsedFilter(const QString &value, const ParsedFilter &pf,
                                    bool exactMatch) const
{
    if (pf.op == FilterOperator::OR) {
        for (const QString &part : pf.parts) {
            if (exactMatch ? (value == part)
                           : value.contains(part, Qt::CaseInsensitive))
                return true;
        }
        return false;
    } else {
        for (const QString &part : pf.parts) {
            if (exactMatch ? (value != part)
                           : !value.contains(part, Qt::CaseInsensitive))
                return false;
        }
        return true;
    }
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

