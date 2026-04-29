#ifndef THREADTIMELOGCONVERTER_H
#define THREADTIMELOGCONVERTER_H

#include "ilogconverter.h"
#include <QRegularExpression>

/**
 * Converter for Android logcat threadtime format (supports two variants)
 * Format 1: MM-DD HH:MM:SS.mmm PID TID LEVEL TAG: message
 * Example: 01-15 10:23:45.123 1234 5678 I MyTag: Log message here
 * Format 2: MM-DD HH:MM:SS.mmm PACKAGE LEVEL/TAG: message
 * Example: 02-03 09:34:57.012 com.android.bluetooth I/BtGatt.ScanManager: msg.what = 6
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
    mutable int m_cachedYear = 0;
    mutable qint64 m_cachedYearStampMs = 0;
};

#endif // THREADTIMELOGCONVERTER_H
