#ifndef PROPERTYDEFINITIONJSON_H
#define PROPERTYDEFINITIONJSON_H

#include <QByteArray>
#include <QJsonDocument>
#include <QString>
#include <QVector>
#include "propertydefinition.h"

/**
 * Serialize a list of PropertyDefinition records to/from the canonical JSON
 * form used both by SQLite preset payloads and exported .json files.
 *
 * Schema:
 *   { "propertyDefinitions": [
 *       { "id":..., "name":..., "isSupported":..., "value":...,
 *         "defaultValue":..., "needReboot":..., "type":..., "readOnly":... }
 *     ]
 *   }
 */
namespace PropertyDefinitionJson {

QJsonDocument toDocument(const QVector<PropertyDefinition> &defs);
QByteArray    toBytes   (const QVector<PropertyDefinition> &defs);

/** Parses @p doc; returns parsed defs or empty + sets errorMsg on failure. */
QVector<PropertyDefinition> fromDocument(const QJsonDocument &doc, QString &errorMsg);
QVector<PropertyDefinition> fromBytes   (const QByteArray   &bytes, QString &errorMsg);

} // namespace PropertyDefinitionJson

#endif // PROPERTYDEFINITIONJSON_H
