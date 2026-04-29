#ifndef PROPERTIESLISTCONVERTER_H
#define PROPERTIESLISTCONVERTER_H

#include <QString>
#include <QVector>
#include "propertyentry.h"

/**
 * Converter for `adb shell getprop` output.
 *
 * Each line has the form:  [property.name]: [value]
 */
class PropertiesListConverter
{
public:
    static QVector<PropertyEntry> convert(const QString &output);
};

#endif // PROPERTIESLISTCONVERTER_H
