#include "valuefilter.h"
#include "filterengine.h"

bool ValueFilter::passesFilter(const QString &name,
                               const QString &value,
                               const ValueFilterCriteria &criteria) const
{
    // Name field: AND of (logic tokens, regex). Either may be inactive.
    if (!FilterEngine::matchesLogic(name, criteria.parsedName))   return false;
    if (!FilterEngine::matchesRegex(name, criteria.nameRegex))    return false;

    // Value field: same combination.
    if (!FilterEngine::matchesLogic(value, criteria.parsedValue)) return false;
    if (!FilterEngine::matchesRegex(value, criteria.valueRegex))  return false;

    return true;
}
