#ifndef VALUEFILTER_H
#define VALUEFILTER_H

#include <QString>
#include <QRegularExpression>

#include "ilogfilter.h"   // brings in ParsedFilter / FilterOperator

// Generic filter criteria for any "name + value" tabular config (settings,
// system properties, property definitions, ...). Two matching modes are
// supported per field, mirroring FilterEngine's primitives:
//
//   - Logic   (`&&` / `||` token list) - pre-parsed into ParsedFilter.
//   - Regex   - compiled QRegularExpression. Empty pattern == no constraint.
//
// Callers populate either the `parsed*` token caches OR the `*Regex` fields
// (or both - they're combined with AND). The raw `nameFilter` /
// `valueFilter` strings are kept for diagnostics; matching itself never
// re-parses them.
struct ValueFilterCriteria {
    QString            nameFilter;
    ParsedFilter       parsedName;
    QRegularExpression nameRegex;

    QString            valueFilter;
    ParsedFilter       parsedValue;
    QRegularExpression valueRegex;
};

// Stateless name+value filter - delegates all matching to FilterEngine
// primitives. Used by SettingsModel, PropertiesModel, and any future table
// whose rows reduce to a (name, value) pair.
class ValueFilter
{
public:
    ValueFilter() = default;

    bool passesFilter(const QString &name,
                      const QString &value,
                      const ValueFilterCriteria &criteria) const;
};

#endif // VALUEFILTER_H
