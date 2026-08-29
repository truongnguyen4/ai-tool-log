#include "propertydefinitionquery.h"

PropertyDefinitionQuery PropertyDefinitionQuery::parse(const QString &text)
{
    PropertyDefinitionQuery query;
    query.m_query = FilterQuery::parse(text, schema());
    return query;
}

const FilterQuery::Schema &PropertyDefinitionQuery::schema()
{
    static const FilterQuery::Schema columns = {
        {int(Field::Name),    {QStringLiteral("name"), QStringLiteral("prop"),
                               QStringLiteral("property"), QStringLiteral("key")}},
        {int(Field::Id),      {QStringLiteral("id")}},
        {int(Field::Type),    {QStringLiteral("type")}},
        {int(Field::Value),   {QStringLiteral("value"), QStringLiteral("val")}},
        {int(Field::Default), {QStringLiteral("default"), QStringLiteral("def")}},
    };
    return columns;
}

bool PropertyDefinitionQuery::matches(const PropertyDefinition &def, const QString &value) const
{
    return m_query.matches([&](const FilterQuery::Term &term) {
        const auto columnMatches = [&term](const QString &column) {
            return FilterQuery::textMatches(column, term.text, term.match);
        };
        switch (Field(term.field)) {
        case Field::Any:
            return columnMatches(def.name) || columnMatches(value);
        case Field::Name:
            return columnMatches(def.name);
        case Field::Id:
            // "id:8" finding 80 and 800 would never be what was meant.
            return QString::number(def.id) == term.text.trimmed();
        case Field::Type:
            return columnMatches(def.type) || columnMatches(def.restrictionName());
        case Field::Value:
            return columnMatches(value);
        case Field::Default:
            return columnMatches(def.defaultValue);
        }
        return false;
    });
}
