#include "filterquery.h"

#include <utility>

namespace {

using Match = FilterQuery::Match;

/** Groups nested deeper than this are read as if their parentheses were absent. */
constexpr int kMaxGroupDepth = 32;

/** Characters that end a bare word. */
bool isSyntaxChar(QChar c)
{
    return c == u'(' || c == u')' || c == u'|' || c == u'&' || c == u'"';
}

/** The field @p key names in @p schema, or -1 when it names none. */
int fieldForKey(const FilterQuery::Schema &schema, QStringView key)
{
    for (const FilterQuery::Column &column : schema) {
        for (const QString &spelling : column.keys) {
            if (key.compare(spelling, Qt::CaseInsensitive) == 0)
                return column.field;
        }
    }
    return -1;
}

/** The spelling written back for @p field. */
QString keyForField(const FilterQuery::Schema &schema, int field)
{
    for (const FilterQuery::Column &column : schema) {
        if (column.field == field && !column.keys.isEmpty())
            return column.keys.first();
    }
    return QString();
}

bool isSameValue(const QString &a, const QString &b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

/** @p value as written in a query: bare where the syntax allows, quoted otherwise. */
QString quoted(const QString &value, bool bareWord)
{
    // A bare word must not read as an exclusion, and a bare word holding a key
    // separator could read as a column key.
    bool needsQuotes = value.isEmpty() || value.startsWith(u'-')
                       || (bareWord && (value.contains(u':') || value.contains(u'=')));
    for (qsizetype i = 0; !needsQuotes && i < value.size(); ++i)
        needsQuotes = value.at(i).isSpace() || isSyntaxChar(value.at(i));
    if (!needsQuotes)
        return value;

    QString result;
    result.reserve(value.size() + 2);
    result += u'"';
    for (QChar c : value) {
        if (c == u'"' || c == u'\\')
            result += u'\\';
        result += c;
    }
    result += u'"';
    return result;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Parser — tokens first, then recursive descent: conditions (ANDed) made of
// alternatives (ORed) of optionally negated terms and groups. Every node
// records the span of text it came from.
// ─────────────────────────────────────────────────────────────────────────────

class FilterQuery::Parser
{
public:
    Parser(FilterQuery &query, const Schema &schema) : m_query(query), m_schema(schema) {}

    void run();

private:
    struct Token {
        enum class Type { Word, Phrase, Key, Or, And, Not, Open, Close, End };
        Type    type  = Type::End;
        QString text;                     ///< Word / Phrase: the value
        int     field = kAnyField;        ///< Key: the column named
        Match   match = Match::Contains;  ///< Key: ":" or "="
        int     begin = 0;
        int     end   = 0;
    };
    using Type = Token::Type;

    /** The column, and how to match it, for terms without a key of their own. */
    struct Scope {
        int   field = kAnyField;
        Match match = Match::Contains;
    };

    void tokenize();
    void addToken(Type type, int begin, int end, const QString &text = QString(),
                  int field = kAnyField, Match match = Match::Contains);
    const Token &peek() const { return m_tokens.at(m_position); }
    Token take();

    int parseAnd(Scope scope, int depth);
    int parseOr(Scope scope, int depth);
    int parseUnary(Scope scope, int depth);
    int parsePrimary(Scope scope, int depth);
    int parseGroup(Scope scope, int depth, int begin);

    int addTerm(Scope scope, const QString &text, int begin, int end);
    int addNot(int operand, int begin);
    int combine(Node::Kind kind, const QVector<int> &operands);
    void warn(const QString &message);

    FilterQuery    &m_query;
    const Schema   &m_schema;
    QVector<Token>  m_tokens;
    int             m_position = 0;
};

void FilterQuery::Parser::run()
{
    tokenize();

    // A ")" with no "(" is reported and skipped; whatever follows still counts.
    QVector<int> conditions;
    for (;;) {
        const int condition = parseAnd(Scope(), 0);
        if (condition >= 0)
            conditions.append(condition);
        if (peek().type != Type::Close)
            break;
        take();
        warn(tr("Unmatched ')' — ignored"));
    }
    m_query.m_root = combine(Node::Kind::And, conditions);
}

void FilterQuery::Parser::tokenize()
{
    const QString &source = m_query.m_text;
    const int length = int(source.size());
    int i = 0;

    while (i < length) {
        const QChar c = source.at(i);
        const int begin = i;

        if (c.isSpace()) {
            ++i;
            continue;
        }
        if (c == u'(' || c == u')') {
            ++i;
            addToken(c == u'(' ? Type::Open : Type::Close, begin, i);
            continue;
        }
        if (c == u'|' || c == u'&') {
            ++i;
            if (i < length && source.at(i) == c)
                ++i;   // "||" and "&&" read the same as "|" and "&"
            addToken(c == u'|' ? Type::Or : Type::And, begin, i);
            continue;
        }
        if (c == u'"') {
            QString text;
            bool closed = false;
            for (++i; i < length; ++i) {
                QChar ch = source.at(i);
                if (ch == u'"') {
                    closed = true;
                    ++i;
                    break;
                }
                // Inside a phrase, \" and \\ stand for the character itself.
                if (ch == u'\\' && i + 1 < length
                    && (source.at(i + 1) == u'"' || source.at(i + 1) == u'\\'))
                    ch = source.at(++i);
                text += ch;
            }
            if (!closed) {
                m_query.m_unterminatedQuote = true;
                warn(tr("Missing closing quote — the phrase runs to the end"));
            }
            addToken(Type::Phrase, begin, i, text);
            continue;
        }
        if (c == u'-' && i + 1 < length) {
            // "-" starts an exclusion only when a term follows it directly;
            // standing alone it is an ordinary character to search for.
            const QChar next = source.at(i + 1);
            if (!next.isSpace() && next != u')' && next != u'|' && next != u'&') {
                ++i;
                addToken(Type::Not, begin, i);
                continue;
            }
        }

        while (i < length && !source.at(i).isSpace() && !isSyntaxChar(source.at(i)))
            ++i;
        const QStringView word = QStringView(source).mid(begin, i - begin);

        // "key:value" or "key=value", when the part before the first separator
        // is a key of this table; otherwise the separator is ordinary text.
        qsizetype separator = -1;
        for (qsizetype k = 0; k < word.size() && separator < 0; ++k) {
            if (word.at(k) == u':' || word.at(k) == u'=')
                separator = k;
        }
        const int field = separator > 0 ? fieldForKey(m_schema, word.left(separator)) : -1;
        if (field >= 0) {
            const Match match = word.at(separator) == u'=' ? Match::Exact : Match::Contains;
            const int valueBegin = begin + int(separator) + 1;
            addToken(Type::Key, begin, valueBegin, QString(), field, match);
            if (valueBegin < i)
                addToken(Type::Word, valueBegin, i, source.mid(valueBegin, i - valueBegin));
            continue;
        }
        addToken(Type::Word, begin, i, word.toString());
    }
    addToken(Type::End, length, length);
}

void FilterQuery::Parser::addToken(Type type, int begin, int end, const QString &text,
                                   int field, Match match)
{
    Token token;
    token.type  = type;
    token.text  = text;
    token.field = field;
    token.match = match;
    token.begin = begin;
    token.end   = end;
    m_tokens.append(token);
}

FilterQuery::Parser::Token FilterQuery::Parser::take()
{
    const Token token = m_tokens.at(m_position);
    if (token.type != Type::End)
        ++m_position;
    return token;
}

int FilterQuery::Parser::parseAnd(Scope scope, int depth)
{
    // Conditions, separated by spaces or "&". This is the loosest level, so in
    // "camera | wifi -tag:chatty" the exclusion applies to both alternatives.
    QVector<int> operands;
    bool awaitingOperand = false;   // an explicit "&" still needs its right side

    for (;;) {
        const Type type = peek().type;
        if (type == Type::End || type == Type::Close)
            break;
        if (type == Type::And) {
            take();
            if (operands.isEmpty() || awaitingOperand)
                warn(tr("'&' needs a term on both sides — ignored"));
            awaitingOperand = true;
            continue;
        }
        if (type == Type::Or) {
            // A "|" with no alternative before it, as in "| camera".
            take();
            warn(tr("'|' needs a term on both sides — ignored"));
            continue;
        }
        const int operand = parseOr(scope, depth);
        if (operand >= 0) {
            operands.append(operand);
            awaitingOperand = false;
        }
    }
    if (awaitingOperand)
        warn(tr("'&' needs a term on both sides — ignored"));
    return combine(Node::Kind::And, operands);
}

int FilterQuery::Parser::parseOr(Scope scope, int depth)
{
    // Alternatives joined by "|", which binds tighter than the separators
    // between conditions.
    QVector<int> operands;
    const int first = parseUnary(scope, depth);
    if (first >= 0)
        operands.append(first);

    while (peek().type == Type::Or) {
        take();
        const int operand = parseUnary(scope, depth);
        if (operand < 0)
            warn(tr("'|' needs a term on both sides — ignored"));
        else
            operands.append(operand);
    }
    return combine(Node::Kind::Or, operands);
}

int FilterQuery::Parser::parseUnary(Scope scope, int depth)
{
    const int begin = peek().begin;
    bool sawNot = false;
    bool negated = false;
    while (peek().type == Type::Not) {
        take();
        sawNot = true;
        negated = !negated;
    }

    const int operand = parsePrimary(scope, depth);
    if (operand < 0) {
        if (sawNot)
            warn(tr("'-' needs a term after it — ignored"));
        return -1;
    }
    return negated ? addNot(operand, begin) : operand;
}

int FilterQuery::Parser::parsePrimary(Scope scope, int depth)
{
    switch (peek().type) {
    case Type::Word:
    case Type::Phrase: {
        const Token value = take();
        return addTerm(scope, value.text, value.begin, value.end);
    }
    case Type::Key: {
        const Token key = take();
        const Token &next = peek();
        const Scope keyScope{key.field, key.match};
        // The value has to follow the key directly. "pid: 1234" is far more
        // likely text copied from a log line than a column filter, so it is
        // reported rather than guessed at.
        if (next.begin == key.end) {
            if (next.type == Type::Word || next.type == Type::Phrase) {
                const Token value = take();
                return addTerm(keyScope, value.text, key.begin, value.end);
            }
            if (next.type == Type::Open) {
                take();
                return parseGroup(keyScope, depth, key.begin);
            }
        }
        warn(tr("'%1' needs a value right after it — ignored")
                 .arg(m_query.m_text.mid(key.begin, key.end - key.begin)));
        return -1;
    }
    case Type::Open:
        return parseGroup(scope, depth, take().begin);
    default:
        return -1;
    }
}

int FilterQuery::Parser::parseGroup(Scope scope, int depth, int begin)
{
    if (depth >= kMaxGroupDepth) {
        // Stop recursing: the contents are read as part of the enclosing group.
        warn(tr("Too many nested ( ) — the innermost are ignored"));
        return -1;
    }

    const int inner = parseAnd(scope, depth + 1);
    int end = int(m_query.m_text.size());
    if (peek().type == Type::Close)
        end = take().end;
    else
        warn(tr("Missing ')' — closed at the end"));

    if (inner < 0) {
        warn(tr("Empty ( ) — ignored"));
        return -1;
    }
    // Widen the span over "key:(" and ")", so a rewrite replaces all of it.
    Node &node = m_query.m_nodes[inner];
    node.begin = begin;
    node.end   = end;
    return inner;
}

int FilterQuery::Parser::addTerm(Scope scope, const QString &text, int begin, int end)
{
    // An empty value only means something as key="": the column is empty.
    if (text.isEmpty() && scope.match != Match::Exact) {
        warn(tr("Empty \"\" — ignored"));
        return -1;
    }

    Node node;
    node.kind  = Node::Kind::Term;
    node.term  = Term{scope.field, text, scope.match};
    node.begin = begin;
    node.end   = end;
    m_query.m_nodes.append(node);
    return int(m_query.m_nodes.size()) - 1;
}

int FilterQuery::Parser::addNot(int operand, int begin)
{
    Node node;
    node.kind  = Node::Kind::Not;
    node.first = operand;
    node.begin = begin;
    node.end   = m_query.m_nodes.at(operand).end;
    m_query.m_nodes.append(node);
    return int(m_query.m_nodes.size()) - 1;
}

int FilterQuery::Parser::combine(Node::Kind kind, const QVector<int> &operands)
{
    if (operands.isEmpty())
        return -1;
    if (operands.size() == 1)
        return operands.first();

    QVector<Node> &nodes = m_query.m_nodes;
    QVector<int>  &list  = m_query.m_operands;

    Node node;
    node.kind  = kind;
    node.first = int(list.size());
    node.begin = nodes.at(operands.first()).begin;
    node.end   = nodes.at(operands.last()).end;
    for (int operand : operands) {
        const Node &child = nodes.at(operand);
        if (child.kind != kind) {
            list.append(operand);
            continue;
        }
        // "(a b) c" is "a b c": splice in the operands of a nested node of the
        // same kind, which keeps evaluation flat.
        for (int i = child.first; i < child.first + child.count; ++i) {
            const int nested = list.at(i);
            list.append(nested);
        }
    }
    node.count = int(list.size()) - node.first;
    nodes.append(node);
    return int(nodes.size()) - 1;
}

void FilterQuery::Parser::warn(const QString &message)
{
    if (m_query.m_warning.isEmpty())
        m_query.m_warning = message;
}

// ─────────────────────────────────────────────────────────────────────────────
// FilterQuery
// ─────────────────────────────────────────────────────────────────────────────

FilterQuery FilterQuery::parse(const QString &text, const Schema &schema)
{
    FilterQuery query;
    query.m_text = text;
    Parser(query, schema).run();
    return query;
}

QVector<FilterQuery::Term> FilterQuery::positiveTerms() const
{
    QVector<Term> terms;
    if (m_root >= 0)
        collectPositiveTerms(m_root, /*negated=*/false, terms);
    return terms;
}

void FilterQuery::collectPositiveTerms(int index, bool negated, QVector<Term> &out) const
{
    const Node &node = m_nodes.at(index);
    switch (node.kind) {
    case Node::Kind::Term:
        if (!negated)
            out.append(node.term);
        return;
    case Node::Kind::Not:
        collectPositiveTerms(node.first, !negated, out);
        return;
    case Node::Kind::And:
    case Node::Kind::Or:
        for (int i = node.first; i < node.first + node.count; ++i)
            collectPositiveTerms(m_operands.at(i), negated, out);
        return;
    }
}

QString FilterQuery::fieldTerm(const Schema &schema, int field, const QString &value)
{
    const QString key = keyForField(schema, field);
    if (field == kAnyField || key.isEmpty())
        return quoted(value, /*bareWord=*/true);
    QString term = key;
    term += u':';
    term += quoted(value, /*bareWord=*/false);
    return term;
}

FilterQuery::Edit FilterQuery::includeTerm(const QString &query, const Schema &schema,
                                           int field, const QString &value)
{
    const FilterQuery parsed = parse(query, schema);
    const QString key = keyForField(schema, field);

    if (field != kAnyField && !key.isEmpty()) {
        const QVector<int> conditions = parsed.topLevelConditions();
        for (int condition : conditions) {
            QStringList values;
            if (!parsed.includedValues(condition, field, values))
                continue;
            for (const QString &existing : std::as_const(values)) {
                if (isSameValue(existing, value))
                    return {query, Edit::Kind::Unchanged};
            }
            values.append(value);

            // Inside the group every value is read as a bare word again.
            QStringList written;
            for (const QString &each : std::as_const(values))
                written.append(quoted(each, /*bareWord=*/true));
            QString group = key;
            group += QLatin1String(":(");
            group += written.join(QLatin1String(" | "));
            group += u')';

            const Node &node = parsed.m_nodes.at(condition);
            QString text = query;
            text.replace(node.begin, node.end - node.begin, group);
            return {text, Edit::Kind::Merged};
        }
    }
    return {parsed.withCondition(fieldTerm(schema, field, value)), Edit::Kind::Appended};
}

FilterQuery::Edit FilterQuery::excludeTerm(const QString &query, const Schema &schema,
                                           int field, const QString &value)
{
    const FilterQuery parsed = parse(query, schema);

    const QVector<int> conditions = parsed.topLevelConditions();
    for (int condition : conditions) {
        const Node &node = parsed.m_nodes.at(condition);
        if (node.kind != Node::Kind::Not)
            continue;
        const Node &operand = parsed.m_nodes.at(node.first);
        if (operand.kind == Node::Kind::Term && operand.term.field == field
            && operand.term.match == Match::Contains && isSameValue(operand.term.text, value))
            return {query, Edit::Kind::Unchanged};
    }

    QString condition = fieldTerm(schema, field, value);
    condition.prepend(u'-');
    return {parsed.withCondition(condition), Edit::Kind::Appended};
}

QVector<int> FilterQuery::topLevelConditions() const
{
    if (m_root < 0)
        return {};
    const Node &root = m_nodes.at(m_root);
    if (root.kind != Node::Kind::And)
        return {m_root};
    return m_operands.mid(root.first, root.count);
}

bool FilterQuery::includedValues(int index, int field, QStringList &values) const
{
    // Only "key:value" includes widen; "key=value" and exclusions are left alone.
    const auto isInclude = [field](const Node &node) {
        return node.kind == Node::Kind::Term && node.term.field == field
               && node.term.match == Match::Contains;
    };

    const Node &node = m_nodes.at(index);
    if (isInclude(node)) {
        values = QStringList{node.term.text};
        return true;
    }
    if (node.kind != Node::Kind::Or)
        return false;

    QStringList collected;
    for (int i = node.first; i < node.first + node.count; ++i) {
        const Node &operand = m_nodes.at(m_operands.at(i));
        if (!isInclude(operand))
            return false;
        collected.append(operand.term.text);
    }
    values = collected;
    return true;
}

QString FilterQuery::withCondition(const QString &condition) const
{
    QString current = m_text.trimmed();
    if (current.isEmpty())
        return condition;
    if (m_unterminatedQuote)
        current += u'"';

    // A repaired query may still hold the dangling operator it was repaired
    // around ("a |" would swallow the new condition as an alternative), and a
    // bare "a | b" reads more clearly with its alternatives grouped. Wrapping
    // both keeps the new condition applying to everything already there.
    const bool wrap = !m_warning.isEmpty()
                      || (m_root >= 0 && m_nodes.at(m_root).kind == Node::Kind::Or);
    if (wrap) {
        current.prepend(u'(');
        current += u')';
    }
    current += u' ';
    current += condition;
    return current;
}
