#ifndef PROPERTYDEFINITIONCONVERTER_H
#define PROPERTYDEFINITIONCONVERTER_H

#include <QString>
#include <QVector>
#include "propertydefinition.h"

/**
 * Converter for ADB property definition output format
 * Parses output from: adb shell cmd cradle_manager get [property_name]
 * Format: id: 142, name: PROP_NAME, optional: true, persistence: false, ...
 */
class PropertyDefinitionConverter
{
public:
    /**
     * Parse a single line of property definition output
     * @param line Single line from ADB output containing property definition
     * @return Parsed PropertyDefinition object
     */
    static PropertyDefinition convertLine(const QString &line);
    
    /**
     * Parse multiple lines of property definition output
     * @param output Multi-line output from ADB command
     * @return Vector of parsed PropertyDefinition objects
     */
    static QVector<PropertyDefinition> convertOutput(const QString &output);
    
private:
    PropertyDefinitionConverter() = default; // Static-only class
};

#endif // PROPERTYDEFINITIONCONVERTER_H
