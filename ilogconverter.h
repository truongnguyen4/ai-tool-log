#ifndef ILOGCONVERTER_H
#define ILOGCONVERTER_H

#include <QString>
#include <QSharedPointer>

struct LogEntry {
    QString time;
    QString pid;
    QString tid;
    QString package;
    QString level;
    QString tag;
    QString message;
    
    bool isValid() const {
        return !level.isEmpty() && !message.isEmpty();
    }
};

/**
 * Interface for log line converters
 * Each implementation handles a specific log format
 */
class ILogConverter
{
public:
    virtual ~ILogConverter() = default;
    
    /**
     * Convert a log line string to LogEntry
     * @param line The raw log line
     * @return LogEntry if parsing succeeded, invalid LogEntry otherwise
     */
    virtual LogEntry convert(const QString &line) const = 0;
    
    /**
     * Get the name of this converter
     * @return Human-readable name
     */
    virtual QString name() const = 0;
    
    /**
     * Get description of the log format this converter handles
     * @return Format description
     */
    virtual QString formatDescription() const = 0;
};

using LogConverterPtr = QSharedPointer<ILogConverter>;

#endif // ILOGCONVERTER_H
