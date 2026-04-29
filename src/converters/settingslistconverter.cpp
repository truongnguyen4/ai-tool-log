#include "settingslistconverter.h"

#include <QStringList>

QVector<SettingEntry> SettingsListConverter::convert(const QString &output,
                                                     const QString &namespaceName,
                                                     int *startLineNumber)
{
    QVector<SettingEntry> entries;
    int localLine = 1;
    int *line = startLineNumber ? startLineNumber : &localLine;

    const QString groupLabel = namespaceName.isEmpty()
        ? QString()
        : namespaceName.at(0).toUpper() + namespaceName.mid(1);

    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &raw : lines) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty())
            continue;
        const int eq = trimmed.indexOf('=');
        if (eq <= 0)
            continue;

        SettingEntry entry;
        entry.line    = QString::number((*line)++);
        entry.group   = groupLabel;
        entry.setting = trimmed.left(eq);
        entry.value   = trimmed.mid(eq + 1);
        entries.append(entry);
    }

    return entries;
}
