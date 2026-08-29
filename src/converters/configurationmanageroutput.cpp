#include "configurationmanageroutput.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace {

/** Keeps one adb command line well clear of shell and transport limits. */
constexpr int kMaxCommandChars = 3000;
constexpr int kMaxPropertiesPerCommand = 50;
/** A "0, 1, ..., n" restriction larger than this is not expanded. */
constexpr qint64 kMaxExpandedValues = 100000;

QString tr(const char *text)
{
    return QCoreApplication::translate("ConfigurationManagerOutput", text);
}

/** JSON value as the text form the service prints and accepts. */
QString valueText(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::Bool:
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Double:
        return QString::number(value.toInteger());
    case QJsonValue::String:
        return value.toString();
    case QJsonValue::Array:
    case QJsonValue::Object:
        return QString::fromUtf8(QJsonDocument::fromVariant(value.toVariant())
                                     .toJson(QJsonDocument::Compact));
    case QJsonValue::Null:
    case QJsonValue::Undefined:
        break;
    }
    return QString();
}

/** A property name at the start of an indented result line: "NAME(8) = 1". */
QString leadingName(const QString &line, QString *rest)
{
    static const QRegularExpression pattern(QStringLiteral("^([^\\s(:=]+)\\s*(?:\\(\\d+\\))?\\s*(.*)$"));
    const QRegularExpressionMatch match = pattern.match(line);
    if (!match.hasMatch())
        return QString();
    if (rest)
        *rest = match.captured(2).trimmed();
    return match.captured(1);
}

QVector<QStringList> splitCommands(const QString &verb, const QVector<QStringList> &items)
{
    QVector<QStringList> commands;
    QStringList current;
    int chars = 0;
    int count = 0;
    for (const QStringList &item : items) {
        int itemChars = 0;
        for (const QString &part : item)
            itemChars += int(part.size()) + 1;
        if (count > 0 && (count >= kMaxPropertiesPerCommand || chars + itemChars > kMaxCommandChars)) {
            commands.append(current);
            current.clear();
            chars = 0;
            count = 0;
        }
        if (current.isEmpty())
            current << verb;
        current << item;
        chars += itemChars;
        ++count;
    }
    if (count > 0)
        commands.append(current);
    return commands;
}

} // namespace

void PropertyWriteResult::append(const PropertyWriteResult &other)
{
    done << other.done;
    failed << other.failed;
    if (!other.error.isEmpty())
        error = error.isEmpty() ? other.error : error + QLatin1Char('\n') + other.error;
}

namespace ConfigurationManagerOutput {

QVector<PropertyDefinition> parseList(const QByteArray &output, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return QVector<PropertyDefinition>();
    };

    // The array may follow a warning line; anything before it is not JSON.
    const qsizetype start = output.indexOf('[');
    if (start < 0) {
        const QString text = QString::fromUtf8(output).trimmed();
        if (text.contains(QLatin1String("Can't find service"), Qt::CaseInsensitive))
            return fail(tr("This device has no configuration_manager service."));
        return fail(text.isEmpty() ? tr("The device returned no property list.")
                                   : tr("Unexpected output from configuration_manager:\n%1")
                                         .arg(text.left(400)));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output.mid(start), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
        return fail(tr("Could not read the property list: %1").arg(parseError.errorString()));

    const QJsonArray array = document.array();
    QVector<PropertyDefinition> definitions;
    definitions.reserve(array.size());
    for (const QJsonValue &item : array) {
        const QJsonObject object = item.toObject();
        PropertyDefinition def;
        def.name = object.value(QLatin1String("name")).toString().trimmed();
        if (def.name.isEmpty())
            continue;

        def.id              = int(object.value(QLatin1String("id")).toInteger(-1));
        def.type            = object.value(QLatin1String("type")).toString();
        def.typeRestriction = object.value(QLatin1String("type_restriction")).toString();
        def.allowedValues   = parseValuesRestriction(
            object.value(QLatin1String("values_restriction")).toString());
        def.hasRange = object.contains(QLatin1String("min")) && object.contains(QLatin1String("max"));
        if (def.hasRange) {
            def.minimum = object.value(QLatin1String("min")).toInteger();
            def.maximum = object.value(QLatin1String("max")).toInteger();
        }
        def.readOnly   = object.value(QLatin1String("read_only")).toBool();
        def.needReboot = object.value(QLatin1String("need_reboot")).toBool();
        def.available  = object.value(QLatin1String("available")).toBool(true);

        const bool isChar = def.kind() == PropertyDefinition::Kind::Char;
        def.defaultValue = valueText(object.value(QLatin1String("default")));
        if (isChar)
            def.defaultValue = decodeUnicodeEscapes(def.defaultValue);

        // An unavailable property reports the string "null" as its value.
        if (def.available)
            def.value = valueText(object.value(QLatin1String("value")));

        definitions.append(def);
    }

    if (definitions.isEmpty() && !array.isEmpty())
        return fail(tr("The property list had no readable entries."));
    if (error)
        error->clear();
    return definitions;
}

QVector<qint64> parseValuesRestriction(const QString &text)
{
    QString body = text.trimmed();
    if (!body.startsWith(QLatin1Char('[')) || !body.endsWith(QLatin1Char(']')))
        return {};
    body = body.mid(1, body.size() - 2);

    QVector<qint64> values;
    bool pendingEllipsis = false;
    for (const QString &rawPart : body.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString part = rawPart.trimmed();
        if (part == QLatin1String("...") || part == QStringLiteral("…")) {
            pendingEllipsis = true;
            continue;
        }
        bool ok = false;
        const qint64 number = part.toLongLong(&ok);
        if (!ok)
            return {};
        if (pendingEllipsis && !values.isEmpty()) {
            const qint64 from = values.last() + 1;
            if (number - from > kMaxExpandedValues)
                return {};
            for (qint64 v = from; v < number; ++v)
                values.append(v);
        }
        pendingEllipsis = false;
        values.append(number);
    }
    return values;
}

QString decodeUnicodeEscapes(const QString &text)
{
    if (!text.contains(QLatin1String("\\u")))
        return text;

    QString decoded;
    decoded.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('\\') && i + 5 < text.size()
            && text.at(i + 1) == QLatin1Char('u')) {
            bool ok = false;
            const ushort code = text.mid(i + 2, 4).toUShort(&ok, 16);
            if (ok) {
                decoded += QChar(code);
                i += 5;
                continue;
            }
        }
        decoded += text.at(i);
    }
    return decoded;
}

PropertyWriteResult parseWriteOutput(const QString &output)
{
    PropertyWriteResult result;
    const QStringList lines = output.split(QLatin1Char('\n'));

    QString section;
    bool sectionSucceeded = false;
    QStringList unrecognized;
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        line.remove(QLatin1Char('\r'));
        if (line.trimmed().isEmpty())
            continue;

        if (line.startsWith(QLatin1String("Exception occurred"))) {
            // The exception's own message is the line that follows.
            QString message = line.trimmed();
            if (i + 1 < lines.size() && !lines.at(i + 1).trimmed().isEmpty())
                message += QLatin1Char('\n') + lines.at(i + 1).trimmed();
            result.error = message;
            break;
        }
        if (line.startsWith(QLatin1String("Error:"))) {
            result.error = line.mid(6).trimmed();
            break;
        }

        const bool indented = line.at(0).isSpace();
        if (!indented && line.trimmed().endsWith(QLatin1Char(':'))) {
            section = line.trimmed().chopped(1);
            sectionSucceeded = section.startsWith(QLatin1String("Successfully"), Qt::CaseInsensitive);
            continue;
        }

        if (indented && !section.isEmpty()) {
            QString rest;
            const QString name = leadingName(line.trimmed(), &rest);
            if (name.isEmpty())
                continue;
            if (sectionSucceeded) {
                result.done << name;
            } else {
                if (rest.startsWith(QLatin1Char(':')))
                    rest = rest.mid(1).trimmed();
                result.failed.append({name, rest.isEmpty() ? section
                                                           : section + QStringLiteral(": ") + rest});
            }
            continue;
        }

        unrecognized << line.trimmed();
    }

    if (result.error.isEmpty() && result.done.isEmpty() && result.failed.isEmpty()
        && !unrecognized.isEmpty())
        result.error = unrecognized.join(QLatin1Char('\n'));
    return result;
}

QString shellQuote(const QString &value)
{
    static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_.,:/@%+=-]+$"));
    if (plain.match(value).hasMatch())
        return value;
    QString quoted = value;
    quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

QVector<QStringList> setCommands(const QVector<QPair<QString, QString>> &values)
{
    QVector<QStringList> items;
    items.reserve(values.size());
    for (const auto &[name, value] : values)
        items.append({name, shellQuote(value)});
    return splitCommands(QStringLiteral("set"), items);
}

QVector<QStringList> resetCommands(const QStringList &names)
{
    QVector<QStringList> items;
    items.reserve(names.size());
    for (const QString &name : names)
        items.append({name});
    return splitCommands(QStringLiteral("reset"), items);
}

} // namespace ConfigurationManagerOutput
