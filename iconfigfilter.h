#ifndef ICONFIGFILTER_H
#define ICONFIGFILTER_H

#include <QString>

enum class ConfigFilterOperator {
    OR,   // Any keyword matches (||)
    AND   // All keywords must match (&&)
};

struct ConfigFilterCriteria {
    QString nameFilter;
    ConfigFilterOperator nameOperator = ConfigFilterOperator::OR;
};

class IConfigFilter
{
public:
    virtual ~IConfigFilter() = default;
    
    // Check if a name passes the filter
    virtual bool passesFilter(const QString &name, const ConfigFilterCriteria &criteria) const = 0;
};

#endif // ICONFIGFILTER_H
