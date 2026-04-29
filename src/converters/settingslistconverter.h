#ifndef SETTINGSLISTCONVERTER_H
#define SETTINGSLISTCONVERTER_H

#include <QString>
#include <QVector>
#include "settingentry.h"

/**
 * Converter for `adb shell settings list <namespace>` output.
 *
 * Output format is one `key=value` pair per line.  Each parsed entry
 * is tagged with the supplied namespace (capitalised for display).
 */
class SettingsListConverter
{
public:
    // Parse settings from a single namespace.  startLineNumber controls the
    // numeric index assigned to the first emitted entry; it is incremented
    // for each subsequent entry so callers parsing several namespaces in
    // sequence can produce a contiguous "line" column.
    static QVector<SettingEntry> convert(const QString &output,
                                         const QString &namespaceName,
                                         int *startLineNumber = nullptr);
};

#endif // SETTINGSLISTCONVERTER_H
