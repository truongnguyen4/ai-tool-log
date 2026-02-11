#include "logfilter.h"

bool LogFilter::passesFilter(const LogEntry &entry, const FilterCriteria &criteria) const
{
    // Message filter (OR/AND logic like tag and package)
    if (!criteria.messageFilter.isEmpty()) {
        if (!matchesStringFilter(entry.message, criteria.messageFilter, criteria.messageOperator)) {
            return false;
        }
    }
    
    // Time range filter
    if (!criteria.startTime.isEmpty() && entry.time < criteria.startTime) {
        return false;
    }
    if (!criteria.endTime.isEmpty() && entry.time > criteria.endTime) {
        return false;
    }
    
    // Tag filter
    if (!criteria.tagFilter.isEmpty()) {
        if (!matchesStringFilter(entry.tag, criteria.tagFilter, criteria.tagOperator)) {
            return false;
        }
    }
    
    // Package filter
    if (!criteria.packageFilter.isEmpty()) {
        if (!matchesStringFilter(entry.package, criteria.packageFilter, criteria.packageOperator)) {
            return false;
        }
    }
    
    // PID filter
    if (!criteria.pidFilter.isEmpty()) {
        if (!matchesStringFilter(entry.pid, criteria.pidFilter, criteria.pidOperator, true)) {
            return false;
        }
    }
    
    // TID filter
    if (!criteria.tidFilter.isEmpty()) {
        if (!matchesStringFilter(entry.tid, criteria.tidFilter, criteria.tidOperator, true)) {
            return false;
        }
    }
    
    // Level filter
    if (!criteria.minLevel.isEmpty()) {
        QStringList levels = {"V", "D", "I", "W", "E", "A"};
        int entryLevel = levels.indexOf(entry.level);
        int filterLevel = levels.indexOf(criteria.minLevel);
        
        if (entryLevel < filterLevel) {
            return false;
        }
    }
    
    return true;
}

bool LogFilter::matchesStringFilter(const QString &value, const QString &filter, 
                                   FilterOperator op, bool exactMatch) const
{
    // Split by || for OR operator or && for AND operator
    QStringList filterParts;
    
    if (filter.contains("&&")) {
        filterParts = filter.split("&&");
        op = FilterOperator::AND;
    } else if (filter.contains("||")) {
        filterParts = filter.split("||");
        op = FilterOperator::OR;
    } else {
        // Backward compatibility: default to OR with | separator
        filterParts = filter.split("|");
    }
    
    if (op == FilterOperator::OR) {
        // OR logic: at least one keyword must match
        for (const QString &part : filterParts) {
            QString trimmedPart = part.trimmed();
            if (trimmedPart.isEmpty()) continue;
            
            if (exactMatch) {
                if (value == trimmedPart) {
                    return true;
                }
            } else {
                if (value.contains(trimmedPart, Qt::CaseInsensitive)) {
                    return true;
                }
            }
        }
        return false; // No match found
    } else {
        // AND logic: all keywords must match
        for (const QString &part : filterParts) {
            QString trimmedPart = part.trimmed();
            if (trimmedPart.isEmpty()) continue;
            
            if (exactMatch) {
                if (value != trimmedPart) {
                    return false;
                }
            } else {
                if (!value.contains(trimmedPart, Qt::CaseInsensitive)) {
                    return false;
                }
            }
        }
        return true; // All keywords matched
    }
}
