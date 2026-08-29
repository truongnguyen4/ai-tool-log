#ifndef FILTERQUERY_H
#define FILTERQUERY_H

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * The filter-box language shared by the log table and the configuration
 * tables, independent of what a row holds.
 *
 * A table names its columns in a Schema and supplies a matcher that checks
 * one term against one row; FilterQuery parses, repairs, evaluates and
 * rewrites.
 *
 *   camera wifi            both words; a bare word looks in the table's searched columns
 *   camera | wifi          either word ("||" works too)
 *   camera & wifi          both words, spelled out ("&&" works too)
 *   -chatty                rows without the word
 *   "start proc"           a phrase, spaces included
 *   key:value              one column, containing the value
 *   key=value              one column, equal to the value; key="" means empty
 *   key:(a | b)            one column for a whole group (key=(a | b) likewise)
 *   (a b) | c              explicit grouping
 *
 * Keys and matching are case-insensitive; a matcher may compare a column more
 * strictly (the log table compares PIDs whole).
 *
 * "|" binds tighter than the spaces (or "&") between conditions, and "-"
 * tighter still: "camera | wifi -tag:chatty" is (camera or wifi) and not tag
 * chatty. Alternatives-then-conditions is how filters get typed, so the common
 * form needs no parentheses.
 *
 * Parsing never fails. A stray ")", a missing ")" or a dangling operator is
 * repaired the obvious way and reported through warning(), so the filter in
 * force always follows the text in the box.
 *
 * A parsed query is immutable, so matches() may run on several threads at
 * once — which the log table's parallel filter relies on.
 */
class FilterQuery
{
    Q_DECLARE_TR_FUNCTIONS(FilterQuery)

public:
    /** Field id of a bare word. Schemas number their columns from 1. */
    static constexpr int kAnyField = 0;

    enum class Match : quint8 {
        Contains,   ///< key:value, and every bare word
        Exact,      ///< key=value
    };

    /** A column a query may name. */
    struct Column {
        int         field = kAnyField;
        QStringList keys;   ///< lower-case spellings; the first is written back
    };
    using Schema = QVector<Column>;

    /** A value the query looks for, and where. */
    struct Term {
        int     field = kAnyField;
        QString text;
        Match   match = Match::Contains;
    };

    /** A query rewritten by includeTerm() / excludeTerm(). */
    struct Edit {
        enum class Kind {
            Appended,   ///< added as one more condition
            Merged,     ///< joined an existing condition on the same column
            Unchanged,  ///< the query already said this
        };
        QString text;
        Kind    kind = Kind::Unchanged;
    };

    static FilterQuery parse(const QString &text, const Schema &schema);

    const QString &text() const    { return m_text; }
    /** First problem repaired while parsing; empty when the text was clean. */
    const QString &warning() const { return m_warning; }
    /** True when the query places no constraint on a row. */
    bool isEmpty() const           { return m_root < 0; }

    /**
     * Whether a row passes. @p termMatches(const Term &) says whether the row
     * satisfies one term; it is only asked about the terms that decide.
     */
    template <typename TermMatcher>
    bool matches(const TermMatcher &termMatches) const
    {
        return m_root < 0 || evaluate(m_root, termMatches);
    }

    /** The terms that select rows: every term not under a "-". */
    QVector<Term> positiveTerms() const;

    /** Case-insensitive containment, or equality for Match::Exact. */
    static bool textMatches(const QString &value, const QString &needle, Match match)
    {
        return match == Match::Exact ? value.compare(needle, Qt::CaseInsensitive) == 0
                                     : value.contains(needle, Qt::CaseInsensitive);
    }

    /** "key:value", with the value quoted when the syntax requires it. */
    static QString fieldTerm(const Schema &schema, int field, const QString &value);

    /**
     * @p query narrowed to rows whose @p field contains @p value. A top-level
     * condition on the same column is widened instead — "tag:a" becomes
     * "tag:(a | value)" — because the columns this is offered for hold one
     * value per row, where ANDing two would match nothing.
     */
    static Edit includeTerm(const QString &query, const Schema &schema,
                            int field, const QString &value);
    /** @p query with "-key:value" added, unless that exclusion is already there. */
    static Edit excludeTerm(const QString &query, const Schema &schema,
                            int field, const QString &value);

private:
    class Parser;

    struct Node {
        enum class Kind : quint8 { Term, And, Or, Not };
        Kind kind  = Kind::Term;
        Term term;           ///< Term nodes: what to look for
        int  first = -1;     ///< And/Or: offset into m_operands; Not: the operand
        int  count = 0;      ///< And/Or: number of operands
        int  begin = 0;      ///< span in m_text, used when rewriting the query
        int  end   = 0;
    };

    template <typename TermMatcher>
    bool evaluate(int index, const TermMatcher &termMatches) const
    {
        const Node &node = m_nodes.at(index);
        switch (node.kind) {
        case Node::Kind::Term:
            return termMatches(node.term);
        case Node::Kind::Not:
            return !evaluate(node.first, termMatches);
        case Node::Kind::And:
            for (int i = node.first; i < node.first + node.count; ++i) {
                if (!evaluate(m_operands.at(i), termMatches))
                    return false;
            }
            return true;
        case Node::Kind::Or:
            for (int i = node.first; i < node.first + node.count; ++i) {
                if (evaluate(m_operands.at(i), termMatches))
                    return true;
            }
            return false;
        }
        return false;
    }

    void collectPositiveTerms(int node, bool negated, QVector<Term> &out) const;
    /** The conditions ANDed at the top level: the root's operands, or the root. */
    QVector<int> topLevelConditions() const;
    /** The values of a condition that only includes values of @p field. */
    bool includedValues(int node, int field, QStringList &values) const;
    /** The query text with @p condition added as one more AND condition. */
    QString withCondition(const QString &condition) const;

    QString       m_text;
    QString       m_warning;
    QVector<Node> m_nodes;
    QVector<int>  m_operands;
    int           m_root = -1;
    bool          m_unterminatedQuote = false;
};

#endif // FILTERQUERY_H
