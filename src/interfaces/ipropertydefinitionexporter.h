#ifndef IPROPERTYDEFINITIONEXPORTER_H
#define IPROPERTYDEFINITIONEXPORTER_H

#include <QString>
#include <QVector>
#include "propertydefinition.h"

/**
 * IPropertyDefinitionExporter
 *
 * Abstract interface for serialising/deserialising PropertyDefinition collections
 * to/from a portable file format. Swap to a different subclass to change from
 * JSON to XML, CSV, etc., without touching any call sites.
 */
class IPropertyDefinitionExporter
{
public:
    virtual ~IPropertyDefinitionExporter() = default;

    /**
     * Serialise @p definitions to the file at @p filePath.
     * @return true on success; @p errorMsg is populated on failure.
     */
    virtual bool exportToFile(const QString &filePath,
                              const QVector<PropertyDefinition> &definitions,
                              QString &errorMsg) = 0;

    /**
     * Deserialise definitions from the file at @p filePath into @p outDefinitions.
     * @return true on success; @p errorMsg is populated on failure.
     */
    virtual bool importFromFile(const QString &filePath,
                                QVector<PropertyDefinition> &outDefinitions,
                                QString &errorMsg) = 0;
};

#endif // IPROPERTYDEFINITIONEXPORTER_H
