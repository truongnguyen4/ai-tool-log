#include "propertydefinitionconverter.h"
#include <QRegularExpression>

PropertyDefinition PropertyDefinitionConverter::parseLine(const QString &line)
{
    PropertyDefinition prop;
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return prop;

    // Key normalisation: toLower().remove('_') collapses is_supported→issupported etc.
    // Only the canonical camelCase forms need to appear in the pattern.
    // Pattern matches: key: value, where value is everything up to the next ", key:" or end
    static const QString KEYS =
        "id|name|isSupported|value|default|needReboot|type|readOnly";
    // .*? (instead of [^,]*?) lets values contain commas; \b in lookahead prevents partial key matches
    static const QRegularExpression re(
        QString(R"(\b(%1)\s*:\s*(.*?)(?=\s*,\s*(?:%1)\b\s*:|$))").arg(KEYS),
        QRegularExpression::CaseInsensitiveOption);

    QHash<QString, QString> kv;
    QRegularExpressionMatchIterator it = re.globalMatch(trimmed);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        // Normalise key once: lowercase + drop underscores
        kv.insert(m.captured(1).toLower().remove(QLatin1Char('_')), m.captured(2).trimmed());
    }

    // Normalised keys: id, name, issupported, value, default, needreboot, type, readonly
    auto boolVal = [](const QString &s) {
        return s.toLower() == QLatin1String("true") || s == QLatin1Char('1');
    };

    if (kv.contains("id"))          prop.id          = kv["id"];
    if (kv.contains("name"))        prop.name        = kv["name"];
    if (kv.contains("issupported")) prop.isSupported = boolVal(kv["issupported"]);
    if (kv.contains("value"))       prop.value       = kv["value"];
    if (kv.contains("default"))     prop.defaultValue = kv["default"];
    if (kv.contains("needreboot"))  prop.needReboot  = boolVal(kv["needreboot"]);
    if (kv.contains("type"))        prop.type        = kv["type"];
    if (kv.contains("readonly"))    prop.readOnly    = boolVal(kv["readonly"]);

    return prop;
}

QVector<PropertyDefinition> PropertyDefinitionConverter::parseOutput(const QString &output)
{
    QVector<PropertyDefinition> propertyDefinitions;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        PropertyDefinition prop = parseLine(line);
        
        // Add property if it has at minimum a name
        if (prop.isValid()) {
            propertyDefinitions.append(prop);
        }
    }
    
    return propertyDefinitions;
}
