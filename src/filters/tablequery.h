#ifndef TABLEQUERY_H
#define TABLEQUERY_H

#include <QString>

#include "filterquery.h"

/**
 * The filter box of the Settings and Properties tables: FilterQuery (see
 * filterquery.h for the syntax) over a name / value row, plus the namespace
 * for settings.
 *
 *   wifi          in the name, the value or the namespace
 *   name:wifi     name contains       (also setting:, prop:, property:, key:)
 *   value=1       value is exactly 1  (also val)
 *   value=""      value is empty
 *   ns:secure     settings namespace — system, secure or global
 *                 (also group:, namespace:)
 *
 * These tables hold short values — 0/1, true/false, small numbers — so exact
 * matching matters here: value:1 also matches 10 and 0.1, value=1 does not.
 */
class TableQuery
{
public:
    enum class Table { Settings, Properties };

    enum class Field : quint8 {
        Any = FilterQuery::kAnyField,
        Name,
        Value,
        Namespace,
    };

    static TableQuery parse(const QString &text, Table table);

    const QString &text() const    { return m_query.text(); }
    /** First problem repaired while parsing; empty when the text was clean. */
    const QString &warning() const { return m_query.warning(); }
    /** True when the query places no constraint on a row. */
    bool isEmpty() const           { return m_query.isEmpty(); }

    /** Whether a row passes. @p nameSpace is empty for properties. */
    bool matches(const QString &name, const QString &value,
                 const QString &nameSpace = QString()) const
    {
        return m_query.matches([&](const FilterQuery::Term &term) {
            const auto columnMatches = [&term](const QString &column) {
                return FilterQuery::textMatches(column, term.text, term.match);
            };
            switch (Field(term.field)) {
            case Field::Any:
                return columnMatches(name) || columnMatches(value) || columnMatches(nameSpace);
            case Field::Name:
                return columnMatches(name);
            case Field::Value:
                return columnMatches(value);
            case Field::Namespace:
                return columnMatches(nameSpace);
            }
            return false;
        });
    }

private:
    FilterQuery m_query;
};

#endif // TABLEQUERY_H
