#ifndef PROPERTYDEFINITIONQUERY_H
#define PROPERTYDEFINITIONQUERY_H

#include <QString>

#include "filterquery.h"
#include "propertydefinition.h"

/**
 * The filter box of the SDK property table: FilterQuery (see filterquery.h
 * for the syntax) over configuration_manager properties.
 *
 *   wifi            in the name or the value
 *   name:scan       name contains       (also prop:, property:, key:)
 *   id=8            the property with id 8; ids always compare whole
 *   type:enum       type contains, or its restriction class does
 *   value=true      value is exactly true  (also val:) — a staged value counts
 *   default:0       default contains       (also def:)
 */
class PropertyDefinitionQuery
{
public:
    enum class Field : quint8 {
        Any = FilterQuery::kAnyField,
        Name,
        Id,
        Type,
        Value,
        Default,
    };

    static PropertyDefinitionQuery parse(const QString &text);
    static const FilterQuery::Schema &schema();

    const QString &text() const    { return m_query.text(); }
    /** First problem repaired while parsing; empty when the text was clean. */
    const QString &warning() const { return m_query.warning(); }
    /** True when the query places no constraint on a row. */
    bool isEmpty() const           { return m_query.isEmpty(); }

    /** Whether @p def passes, showing @p value (its staged value, if any). */
    bool matches(const PropertyDefinition &def, const QString &value) const;

private:
    FilterQuery m_query;
};

#endif // PROPERTYDEFINITIONQUERY_H
