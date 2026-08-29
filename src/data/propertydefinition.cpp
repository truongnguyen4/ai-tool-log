#include "propertydefinition.h"

#include <limits>
#include <optional>

namespace {

std::optional<bool> parseBoolean(const QString &text)
{
    const QString t = text.trimmed().toLower();
    if (t == QLatin1String("true") || t == QLatin1String("1")
        || t == QLatin1String("yes") || t == QLatin1String("on"))
        return true;
    if (t == QLatin1String("false") || t == QLatin1String("0")
        || t == QLatin1String("no") || t == QLatin1String("off"))
        return false;
    return std::nullopt;
}

std::optional<qint64> parseNumber(const QString &text)
{
    bool ok = false;
    const qint64 number = text.trimmed().toLongLong(&ok);
    return ok ? std::optional<qint64>(number) : std::nullopt;
}

bool isNumeric(PropertyDefinition::Kind kind)
{
    return kind == PropertyDefinition::Kind::Integer || kind == PropertyDefinition::Kind::Enum
           || kind == PropertyDefinition::Kind::MultiChoice;
}

/** A range that only restates the storage type says nothing worth showing. */
bool isInformativeRange(const PropertyDefinition &def)
{
    if (!def.hasRange)
        return false;
    if (def.kind() == PropertyDefinition::Kind::Char)
        return def.minimum > 0 || def.maximum < 0xFFFF;
    return def.minimum > std::numeric_limits<qint32>::min()
           || def.maximum < std::numeric_limits<qint32>::max();
}

/** "0 – 27" for a contiguous list, "0, 2, 8, 10" otherwise. */
QString describeChoices(const QVector<qint64> &values)
{
    if (values.isEmpty())
        return QString();
    bool contiguous = values.size() > 2;
    for (int i = 1; contiguous && i < values.size(); ++i)
        contiguous = values.at(i) == values.at(i - 1) + 1;
    if (contiguous)
        return QStringLiteral("%1 – %2").arg(values.first()).arg(values.last());

    constexpr int kShown = 6;
    QStringList parts;
    for (int i = 0; i < values.size() && i < kShown; ++i)
        parts << QString::number(values.at(i));
    if (values.size() > kShown)
        parts << QStringLiteral("…");
    return parts.join(QStringLiteral(", "));
}

} // namespace

PropertyDefinition::Kind PropertyDefinition::kind() const
{
    const QString t = type.toLower();
    if (t == QLatin1String("boolean"))     return Kind::Boolean;
    if (t == QLatin1String("integer"))     return Kind::Integer;
    if (t == QLatin1String("enum"))        return Kind::Enum;
    if (t == QLatin1String("multichoice")) return Kind::MultiChoice;
    if (t == QLatin1String("char"))        return Kind::Char;
    if (t == QLatin1String("string"))      return Kind::String;
    if (t == QLatin1String("blob"))        return Kind::Blob;
    return Kind::Other;
}

bool PropertyDefinition::sameValue(const QString &a, const QString &b) const
{
    const Kind k = kind();
    if (k == Kind::Boolean) {
        const auto x = parseBoolean(a);
        const auto y = parseBoolean(b);
        if (x && y)
            return *x == *y;
    } else if (isNumeric(k)) {
        const auto x = parseNumber(a);
        const auto y = parseNumber(b);
        if (x && y)
            return *x == *y;
    }
    return a == b;
}

QString PropertyDefinition::validate(const QString &text, QString *normalized) const
{
    if (!available)
        return tr("%1 is not available on this device.").arg(name);
    if (readOnly)
        return tr("%1 is read-only.").arg(name);

    QString result = text;
    switch (kind()) {
    case Kind::Boolean: {
        const auto parsed = parseBoolean(text);
        if (!parsed)
            return tr("%1 takes true or false.").arg(name);
        result = *parsed ? QStringLiteral("true") : QStringLiteral("false");
        break;
    }
    case Kind::Integer:
    case Kind::Enum:
    case Kind::MultiChoice: {
        const auto parsed = parseNumber(text);
        if (!parsed)
            return tr("%1 takes a whole number.").arg(name);
        if (!allowedValues.isEmpty() && !allowedValues.contains(*parsed))
            return tr("%1 takes one of: %2.").arg(name, describeChoices(allowedValues));
        if (hasRange && (*parsed < minimum || *parsed > maximum))
            return tr("%1 takes a number from %2 to %3.").arg(name).arg(minimum).arg(maximum);
        result = QString::number(*parsed);
        break;
    }
    case Kind::Char: {
        if (text.size() != 1)
            return tr("%1 takes exactly one character.").arg(name);
        const qint64 code = text.at(0).unicode();
        if (hasRange && (code < minimum || code > maximum))
            return tr("%1 takes a character with a code from %2 to %3.")
                       .arg(name).arg(minimum).arg(maximum);
        break;
    }
    case Kind::String:
    case Kind::Blob:
    case Kind::Other:
        break;
    }

    if (normalized)
        *normalized = result;
    return QString();
}

QString PropertyDefinition::restrictionName() const
{
    const int dot = typeRestriction.lastIndexOf(QLatin1Char('.'));
    return dot < 0 ? typeRestriction : typeRestriction.mid(dot + 1);
}

QString PropertyDefinition::typeSummary() const
{
    QString summary = type;
    const QString choices = describeChoices(allowedValues);
    if (!choices.isEmpty())
        summary += QStringLiteral("  ") + choices;
    else if (isInformativeRange(*this))
        summary += QStringLiteral("  %1 – %2").arg(minimum).arg(maximum);
    else if (!typeRestriction.isEmpty())
        summary += QStringLiteral("  ") + restrictionName();
    return summary;
}
