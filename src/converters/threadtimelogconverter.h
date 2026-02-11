#ifndef THREADTIMELOGCONVERTER_H
#define THREADTIMELOGCONVERTER_H

#include "ilogconverter.h"
#include <QRegularExpression>

/**
 * Converter for Android logcat threadtime format
 * Format: MM-DD HH:MM:SS.mmm PID TID LEVEL TAG: message
 * Example: 01-15 10:23:45.123 1234 5678 I MyTag: Log message here
 */
class ThreadtimeLogConverter : public ILogConverter
{
public:
    ThreadtimeLogConverter();
    ~ThreadtimeLogConverter() override = default;
    
    LogEntry convert(const QString &line) const override;
    QString name() const override;
    QString formatDescription() const override;
    
private:
    QRegularExpression m_regex;
};

#endif // THREADTIMELOGCONVERTER_H
