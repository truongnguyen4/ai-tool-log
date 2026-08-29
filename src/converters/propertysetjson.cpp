#include "propertysetjson.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>

namespace {
constexpr auto kFormat       = "format";
constexpr auto kFormatName   = "ToolLogPro property set";
constexpr auto kVersion      = "version";
constexpr int  kVersionValue = 2;
constexpr auto kProperties   = "properties";
constexpr auto kLegacyRoot   = "propertyDefinitions";
constexpr auto kName         = "name";
constexpr auto kId           = "id";
constexpr auto kType         = "type";
constexpr auto kValue        = "value";

QString tr(const char *text)
{
    return QCoreApplication::translate("PropertySetJson", text);
}

/** Value as text whether it was written as a string, a number or a bool. */
QString valueText(const QJsonValue &value)
{
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isDouble())
        return QString::number(value.toInteger());
    return value.toString();
}
} // namespace

namespace PropertySetJson {

QJsonDocument toDocument(const QVector<PropertySetEntry> &entries)
{
    QJsonArray array;
    for (const PropertySetEntry &entry : entries) {
        QJsonObject object;
        object[QLatin1String(kName)] = entry.name;
        if (entry.id >= 0)
            object[QLatin1String(kId)] = entry.id;
        if (!entry.type.isEmpty())
            object[QLatin1String(kType)] = entry.type;
        object[QLatin1String(kValue)] = entry.value;
        array.append(object);
    }
    QJsonObject root;
    root[QLatin1String(kFormat)]     = QLatin1String(kFormatName);
    root[QLatin1String(kVersion)]    = kVersionValue;
    root[QLatin1String(kProperties)] = array;
    return QJsonDocument(root);
}

QByteArray toBytes(const QVector<PropertySetEntry> &entries)
{
    return toDocument(entries).toJson(QJsonDocument::Compact);
}

QVector<PropertySetEntry> fromDocument(const QJsonDocument &doc, QString &errorMsg)
{
    if (!doc.isObject()) {
        errorMsg = tr("Not a property set: the file must hold a JSON object.");
        return {};
    }
    const QJsonObject root = doc.object();
    QJsonValue list = root.value(QLatin1String(kProperties));
    if (!list.isArray())
        list = root.value(QLatin1String(kLegacyRoot));
    if (!list.isArray()) {
        errorMsg = tr("Not a property set: no \"%1\" list found.").arg(QLatin1String(kProperties));
        return {};
    }

    QVector<PropertySetEntry> entries;
    for (const QJsonValue &item : list.toArray()) {
        const QJsonObject object = item.toObject();
        PropertySetEntry entry;
        entry.name = object.value(QLatin1String(kName)).toString().trimmed();
        if (entry.name.isEmpty())
            continue;
        // First-generation files stored the id as a string.
        const QJsonValue id = object.value(QLatin1String(kId));
        bool ok = true;
        entry.id    = id.isString() ? id.toString().toInt(&ok) : int(id.toInteger(-1));
        if (!ok)
            entry.id = -1;
        entry.type  = object.value(QLatin1String(kType)).toString();
        entry.value = valueText(object.value(QLatin1String(kValue)));
        entries.append(entry);
    }
    errorMsg.clear();
    return entries;
}

QVector<PropertySetEntry> fromBytes(const QByteArray &bytes, QString &errorMsg)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMsg = parseError.errorString();
        return {};
    }
    return fromDocument(doc, errorMsg);
}

} // namespace PropertySetJson
