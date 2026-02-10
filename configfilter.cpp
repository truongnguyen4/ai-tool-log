#include "configfilter.h"
#include "QStringList"

bool ConfigFilter::passesFilter(const QString &name, const ConfigFilterCriteria &criteria) const
{
    // Name filter (OR/AND logic)
    if (!criteria.nameFilter.isEmpty()) {
        if (!matchesStringFilter(name, criteria.nameFilter, criteria.nameOperator)) {
            return false;
        }
    }
    
    return true;
}

bool ConfigFilter::matchesStringFilter(const QString &value, const QString &filter, 
                                       ConfigFilterOperator op) const
{
    // Split by || for OR operator or && for AND operator
    QStringList filterParts;
    
    if (filter.contains("&&")) {
        filterParts = filter.split("&&");
        op = ConfigFilterOperator::AND;
    } else if (filter.contains("||")) {
        filterParts = filter.split("||");
        op = ConfigFilterOperator::OR;
    } else {
        // Backward compatibility: default to OR with | separator
        filterParts = filter.split("|");
    }
    
    if (op == ConfigFilterOperator::OR) {
        // OR logic: at least one keyword must match
        for (const QString &part : filterParts) {
            QString trimmedPart = part.trimmed();
            if (trimmedPart.isEmpty()) continue;
            
            if (value.contains(trimmedPart, Qt::CaseInsensitive)) {
                return true;
            }
        }
        return false; // No match found
    } else {
        // AND logic: all keywords must match
        for (const QString &part : filterParts) {
            QString trimmedPart = part.trimmed();
            if (trimmedPart.isEmpty()) continue;
            
            if (!value.contains(trimmedPart, Qt::CaseInsensitive)) {
                return false;
            }
        }
        return true; // All keywords matched
    }
}
