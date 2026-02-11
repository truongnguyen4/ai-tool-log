#ifndef CONFIGFILTER_H
#define CONFIGFILTER_H

#include "iconfigfilter.h"

class ConfigFilter : public IConfigFilter
{
public:
    ConfigFilter() = default;
    ~ConfigFilter() override = default;
    
    bool passesFilter(const QString &name, const ConfigFilterCriteria &criteria) const override;
    
private:
    bool matchesStringFilter(const QString &value, const QString &filter, 
                            ConfigFilterOperator op) const;
};

#endif // CONFIGFILTER_H
