#ifndef IPROPERTYDEFINITIONDATABASE_H
#define IPROPERTYDEFINITIONDATABASE_H

#include <QString>
#include <QStringList>
#include <QVector>
#include "propertydefinition.h"

/**
 * IPropertyDefinitionDatabase
 *
 * Abstract interface for persisting named sets of PropertyDefinition objects
 * to a local database. Implementations can swap the underlying storage engine
 * (SQLite, PostgreSQL, etc.) without touching any call sites.
 */
class IPropertyDefinitionDatabase
{
public:
    virtual ~IPropertyDefinitionDatabase() = default;

    /**
     * Persist all @p definitions under the given @p setName.
     * If a set with that name already exists it is overwritten.
     * @return true on success; @p errorMsg is populated on failure.
     */
    virtual bool savePropertySet(const QString &setName,
                                 const QVector<PropertyDefinition> &definitions,
                                 QString &errorMsg) = 0;

    /**
     * Load the definitions previously saved under @p setName.
     * @return populated vector on success; empty vector + @p errorMsg on failure.
     */
    virtual QVector<PropertyDefinition> loadPropertySet(const QString &setName,
                                                        QString &errorMsg) = 0;

    /**
     * Return the names of all persisted property sets, sorted alphabetically.
     */
    virtual QStringList listPropertySetNames() const = 0;

    /**
     * Delete the property set identified by @p setName.
     * @return true on success.
     */
    virtual bool deletePropertySet(const QString &setName, QString &errorMsg) = 0;
};

#endif // IPROPERTYDEFINITIONDATABASE_H
