#ifndef FILTERENGINE_H
#define FILTERENGINE_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>

#include "ilogfilter.h"   // brings in ParsedFilter + FilterOperator

// Stateless matching primitives shared by every table in the app
// (logs, settings, properties, ...).
//
// Two primitives:
//
//   1. matchesLogic   : `&&` / `||` containment matching using a
//                       pre-parsed token list (ParsedFilter).
//   2. matchesRegex   : QRegularExpression against any of N field values.
//
// Higher-level filters (LogFilter, ValueFilter) are thin compositions of
// these primitives over their respective record types.
class FilterEngine
{
public:
    // Returns true if `value` satisfies the parsed filter.
    //
    // - If `pf` is inactive (no tokens), returns true (no constraint).
    // - OR : at least one token matches.
    // - AND: every token matches.
    // - exactMatch=true   uses equality (used for numeric fields like PID/TID).
    // - exactMatch=false  uses case-insensitive substring containment.
    static bool matchesLogic(const QString &value,
                             const ParsedFilter &pf,
                             bool exactMatch = false);

    // Returns true if `re` is empty/invalid (no constraint), or if the
    // expression matches at least one of the supplied field values.
    static bool matchesRegex(const QStringList &values,
                             const QRegularExpression &re);

    // Convenience overload for a single-value match.
    static bool matchesRegex(const QString &value,
                             const QRegularExpression &re);
};

#endif // FILTERENGINE_H
