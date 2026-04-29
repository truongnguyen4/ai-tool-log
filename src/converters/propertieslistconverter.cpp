#include "propertieslistconverter.h"

#include <QRegularExpression>
#include <QStringList>

QVector<PropertyEntry> PropertiesListConverter::convert(const QString &output)
{
    QVector<PropertyEntry> entries;
    static const QRegularExpression kRow(QStringLiteral("\\[([^\\]]+)\\]:\\s*\\[([^\\]]*)\\]"));

    int line = 1;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &raw : lines) {
        const QRegularExpressionMatch m = kRow.match(raw.trimmed());
        if (!m.hasMatch())
            continue;

        PropertyEntry entry;
        entry.line     = QString::number(line++);
        entry.property = m.captured(1);
        entry.value    = m.captured(2);
        entries.append(entry);
    }

    return entries;
}
