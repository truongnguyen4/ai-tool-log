#ifndef PROPERTYSETJSON_H
#define PROPERTYSETJSON_H

#include <QByteArray>
#include <QJsonDocument>
#include <QString>
#include <QVector>

/** One saved property value. The id and type are kept for reference only. */
struct PropertySetEntry
{
    QString name;
    int     id = -1;
    QString type;
    QString value;
};

/**
 * The JSON form of a property set, shared by SQLite presets and exported
 * .json files:
 *
 *   { "format": "ToolLogPro property set", "version": 2,
 *     "properties": [ { "name": "AIM_ENABLE", "id": 8, "type": "boolean",
 *                       "value": "true" }, ... ] }
 *
 * Reading also accepts the first-generation form, a "propertyDefinitions"
 * array whose entries carried the same name / id / type / value keys.
 */
namespace PropertySetJson {

QJsonDocument toDocument(const QVector<PropertySetEntry> &entries);
QByteArray    toBytes(const QVector<PropertySetEntry> &entries);

/** Entries in @p doc; empty, with @p errorMsg set, when it is not a property set. */
QVector<PropertySetEntry> fromDocument(const QJsonDocument &doc, QString &errorMsg);
QVector<PropertySetEntry> fromBytes(const QByteArray &bytes, QString &errorMsg);

} // namespace PropertySetJson

#endif // PROPERTYSETJSON_H
