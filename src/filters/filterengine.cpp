#include "filterengine.h"

bool FilterEngine::matchesLogic(const QString &value,
                                const ParsedFilter &pf,
                                bool exactMatch)
{
    if (!pf.active)
        return true;

    if (pf.op == FilterOperator::OR) {
        for (const QString &part : pf.parts) {
            if (exactMatch ? (value == part)
                           : value.contains(part, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

    // AND
    for (const QString &part : pf.parts) {
        if (exactMatch ? (value != part)
                       : !value.contains(part, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

bool FilterEngine::matchesRegex(const QStringList &values,
                                const QRegularExpression &re)
{
    if (re.pattern().isEmpty() || !re.isValid())
        return true;

    for (const QString &v : values) {
        if (re.match(v).hasMatch())
            return true;
    }
    return false;
}

bool FilterEngine::matchesRegex(const QString &value,
                                const QRegularExpression &re)
{
    if (re.pattern().isEmpty() || !re.isValid())
        return true;
    return re.match(value).hasMatch();
}
