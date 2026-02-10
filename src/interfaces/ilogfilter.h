#ifndef ILOGFILTER_H
#define ILOGFILTER_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include "ilogconverter.h"

enum class FilterOperator {
    OR,   // Any keyword matches (||)
    AND   // All keywords must match (&&)
};

// Pre-parsed form of a single filter field.
// Built once in buildFilterCriteria(); used directly in passesFilter()
// without any further string splits or allocations.
struct ParsedFilter {
    QStringList    parts;           // pre-trimmed, non-empty tokens
    FilterOperator op    = FilterOperator::OR;
    bool           active = false;

    static ParsedFilter build(const QString &raw) {
        ParsedFilter f;
        if (raw.trimmed().isEmpty()) return f;
        QStringList rawParts;
        if (raw.contains(QLatin1String("&&"))) {
            rawParts = raw.split(QLatin1String("&&"));
            f.op = FilterOperator::AND;
        } else if (raw.contains(QLatin1String("||")))
            rawParts = raw.split(QLatin1String("||"));
        else
            rawParts = raw.split(QLatin1Char('|'));
        for (const QString &p : rawParts) {
            const QString t = p.trimmed();
            if (!t.isEmpty()) f.parts.append(t);
        }
        f.active = !f.parts.isEmpty();
        return f;
    }
};

struct FilterCriteria {
    QString messageFilter;
    FilterOperator messageOperator = FilterOperator::OR;
    QString startTime;
    QString endTime;
    QString tagFilter;
    FilterOperator tagOperator = FilterOperator::OR;
    QString packageFilter;
    FilterOperator packageOperator = FilterOperator::OR;
    QString pidFilter;
    FilterOperator pidOperator = FilterOperator::OR;
    QString tidFilter;
    FilterOperator tidOperator = FilterOperator::OR;
    QString minLevel;
    // Keyword filter – regex matched against tag, message, or package
    QString keywordFilter;
    QRegularExpression keywordRegex;

    // Pre-parsed filter parts (populated by buildFilterCriteria).
    // Using these avoids re-splitting the raw strings on every log entry.
    ParsedFilter parsedMessage;
    ParsedFilter parsedTag;
    ParsedFilter parsedPackage;
    ParsedFilter parsedPid;
    ParsedFilter parsedTid;
    // Pre-computed level index: -1 = no filter; 0=V 1=D 2=I 3=W 4=E 5=A
    int minLevelIndex = -1;

    // Infer operator from filter text: "&&" -> AND, everything else -> OR
    static FilterOperator detectOperator(const QString &filter) {
        return filter.contains("&&") ? FilterOperator::AND : FilterOperator::OR;
    }

    // Convenience: set a filter field and auto-detect its operator
    static void applyFilter(QString &field, FilterOperator &op, const QString &value) {
        field = value;
        op = detectOperator(value);
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
