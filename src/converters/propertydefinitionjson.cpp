#include "propertydefinitionjson.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>

namespace {
constexpr const char *kRoot       = "propertyDefinitions";
constexpr const char *kId         = "id";
constexpr const char *kName       = "name";
constexpr const char *kSupported  = "isSupported";
constexpr const char *kValue      = "value";
constexpr const char *kDefault    = "defaultValue";
constexpr const char *kNeedReboot = "needReboot";
constexpr const char *kType       = "type";
constexpr const char *kReadOnly   = "readOnly";
} // namespace

namespace PropertyDefinitionJson {

QJsonDocument toDocument(const QVector<PropertyDefinition> &defs)
{
    QJsonArray array;
    for (const PropertyDefinition &def : defs) {
        QJsonObject obj;
        obj[QLatin1String(kId)]         = def.id;
        obj[QLatin1String(kName)]       = def.name;
        obj[QLatin1String(kSupported)]  = def.isSupported;
        obj[QLatin1String(kValue)]      = def.value;
        obj[QLatin1String(kDefault)]    = def.defaultValue;
        obj[QLatin1String(kNeedReboot)] = def.needReboot;
        obj[QLatin1String(kType)]       = def.type;
        obj[QLatin1String(kReadOnly)]   = def.readOnly;
        array.append(obj);
    }
    QJsonObject root;
    root[QLatin1String(kRoot)] = array;
    return QJsonDocument(root);
}

QByteArray toBytes(const QVector<PropertyDefinition> &defs)
{
    return toDocument(defs).toJson(QJsonDocument::Compact);
}

QVector<PropertyDefinition> fromDocument(const QJsonDocument &doc, QString &errorMsg)
{
    QVector<PropertyDefinition> result;

    if (!doc.isObject()) {
        errorMsg = QCoreApplication::translate(
            "PropertyDefinitionJson",
            "Invalid file format: root element must be a JSON object.");
        return result;
    }

    const QJsonValue rootVal = doc.object().value(QLatin1String(kRoot));
    if (!rootVal.isArray()) {
        errorMsg = QCoreApplication::translate(
            "PropertyDefinitionJson",
            "Invalid file format: '%1' array not found.").arg(QLatin1String(kRoot));
        return result;
    }

    const QJsonArray array = rootVal.toArray();
    result.reserve(array.size());
    for (const QJsonValue &val : array) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        const QString name = obj.value(QLatin1String(kName)).toString().trimmed();
        if (name.isEmpty()) continue;

        PropertyDefinition def;
        def.id           = obj.value(QLatin1String(kId)).toString();
        def.name         = name;
        def.isSupported  = obj.value(QLatin1String(kSupported)).toBool();
        def.value        = obj.value(QLatin1String(kValue)).toString();
        def.defaultValue = obj.value(QLatin1String(kDefault)).toString();
        def.needReboot   = obj.value(QLatin1String(kNeedReboot)).toBool();
        def.type         = obj.value(QLatin1String(kType)).toString();
        def.readOnly     = obj.value(QLatin1String(kReadOnly)).toBool();
        result.append(def);
    }
    return result;
}

QVector<PropertyDefinition> fromBytes(const QByteArray &bytes, QString &errorMsg)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMsg = parseError.errorString();
        return {};
    }
    return fromDocument(doc, errorMsg);
}

} // namespace PropertyDefinitionJson
