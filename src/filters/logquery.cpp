#include "logquery.h"

const FilterQuery::Schema &LogQuery::schema()
{
    static const FilterQuery::Schema columns = {
        {int(Field::Tag),     {QStringLiteral("tag")}},
        {int(Field::Message), {QStringLiteral("msg"), QStringLiteral("message")}},
        {int(Field::Package), {QStringLiteral("pkg"), QStringLiteral("package")}},
        {int(Field::Pid),     {QStringLiteral("pid")}},
        {int(Field::Tid),     {QStringLiteral("tid")}},
    };
    return columns;
}

LogQuery LogQuery::parse(const QString &text)
{
    LogQuery query;
    query.m_query = FilterQuery::parse(text, schema());
    return query;
}

QVector<LogQuery::Term> LogQuery::positiveTerms() const
{
    const QVector<FilterQuery::Term> generic = m_query.positiveTerms();
    QVector<Term> terms;
    terms.reserve(generic.size());
    for (const FilterQuery::Term &term : generic)
        terms.append(Term{Field(term.field), term.text, term.match});
    return terms;
}

QString LogQuery::fieldTerm(Field field, const QString &value)
{
    return FilterQuery::fieldTerm(schema(), int(field), value);
}

LogQuery::Edit LogQuery::includeTerm(const QString &query, Field field, const QString &value)
{
    return FilterQuery::includeTerm(query, schema(), int(field), value);
}

LogQuery::Edit LogQuery::excludeTerm(const QString &query, Field field, const QString &value)
{
    return FilterQuery::excludeTerm(query, schema(), int(field), value);
}
