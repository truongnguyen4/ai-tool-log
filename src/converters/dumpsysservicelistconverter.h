#ifndef DUMPSYSSERVICELISTCONVERTER_H
#define DUMPSYSSERVICELISTCONVERTER_H

#include <QString>
#include <QStringList>

/**
 * Converter for `adb shell dumpsys -l` output.
 *
 * The first line is a header ("Currently running services:"); subsequent
 * lines list one service name (whitespace-trimmed) per line.  The result
 * is sorted case-insensitively to give a stable display order.
 */
class DumpsysServiceListConverter
{
public:
    static QStringList convert(const QString &output);
};

#endif // DUMPSYSSERVICELISTCONVERTER_H
