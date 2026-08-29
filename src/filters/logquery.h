#ifndef LOGQUERY_H
#define LOGQUERY_H

#include <QString>
#include <QVector>

#include "filterquery.h"
#include "logentry.h"

/**
 * The log filter box: FilterQuery (see filterquery.h for the syntax) over the
 * columns of a log line.
 *
 * Bare words look in Tag and Message. Keys: tag, msg (or message), pkg (or
 * package), pid, tid. PID and TID always compare whole values, so "pid:12"
 * does not match 1234.
 */
class LogQuery
{
public:
    enum class Field : quint8 {
        Any = FilterQuery::kAnyField,   ///< a bare word: Tag or Message
        Tag,
        Message,
        Package,
        Pid,
        Tid,
    };
    using Match = FilterQuery::Match;
    using Edit  = FilterQuery::Edit;

    /** A value the query looks for, and where. */
    struct Term {
        Field   field = Field::Any;
        QString text;
        Match   match = Match::Contains;
    };

    static LogQuery parse(const QString &text);

    const QString &text() const    { return m_query.text(); }
    /** First problem repaired while parsing; empty when the text was clean. */
    const QString &warning() const { return m_query.warning(); }
    /** True when the query places no constraint on a line. */
    bool isEmpty() const           { return m_query.isEmpty(); }

    bool matches(const LogEntry &entry) const
    {
        return m_query.matches([&entry](const FilterQuery::Term &term) {
            return columnMatches(Field(term.field), term.text, term.match, entry);
        });
    }

    /** The terms that select lines: every term not under a "-". */
    QVector<Term> positiveTerms() const;
    static bool termMatches(const Term &term, const LogEntry &entry)
    {
        return columnMatches(term.field, term.text, term.match, entry);
    }

    /** "tag:value", with the value quoted when the syntax requires it. */
    static QString fieldTerm(Field field, const QString &value);
    /** See FilterQuery::includeTerm(). */
    static Edit includeTerm(const QString &query, Field field, const QString &value);
    /** See FilterQuery::excludeTerm(). */
    static Edit excludeTerm(const QString &query, Field field, const QString &value);

private:
    static const FilterQuery::Schema &schema();

    static bool columnMatches(Field field, const QString &text, Match match,
                              const LogEntry &entry)
    {
        switch (field) {
        case Field::Any:
            return FilterQuery::textMatches(entry.tag, text, match)
                   || FilterQuery::textMatches(entry.message, text, match);
        case Field::Tag:
            return FilterQuery::textMatches(entry.tag, text, match);
        case Field::Message:
            return FilterQuery::textMatches(entry.message, text, match);
        case Field::Package:
            return FilterQuery::textMatches(entry.package, text, match);
        case Field::Pid:
            return entry.pid == text;
        case Field::Tid:
            return entry.tid == text;
        }
        return false;
    }

    FilterQuery m_query;
};

#endif // LOGQUERY_H
