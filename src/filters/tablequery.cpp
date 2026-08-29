#include "tablequery.h"

namespace {

using Field = TableQuery::Field;

const FilterQuery::Schema &settingsSchema()
{
    static const FilterQuery::Schema columns = {
        {int(Field::Name),      {QStringLiteral("name"), QStringLiteral("setting"),
                                 QStringLiteral("key")}},
        {int(Field::Value),     {QStringLiteral("value"), QStringLiteral("val")}},
        {int(Field::Namespace), {QStringLiteral("ns"), QStringLiteral("namespace"),
                                 QStringLiteral("group")}},
    };
    return columns;
}

const FilterQuery::Schema &propertiesSchema()
{
    static const FilterQuery::Schema columns = {
        {int(Field::Name),  {QStringLiteral("name"), QStringLiteral("prop"),
                             QStringLiteral("property"), QStringLiteral("key")}},
        {int(Field::Value), {QStringLiteral("value"), QStringLiteral("val")}},
    };
    return columns;
}

} // namespace

TableQuery TableQuery::parse(const QString &text, Table table)
{
    TableQuery query;
    query.m_query = FilterQuery::parse(text, table == Table::Settings ? settingsSchema()
                                                                      : propertiesSchema());
    return query;
}
