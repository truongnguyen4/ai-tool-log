#include "threadtimelogconverter.h"
#include <QDateTime>

ThreadtimeLogConverter::ThreadtimeLogConverter()
{
    // Regex pattern for threadtime format with two variants:
    // 1. Standard: MM-DD HH:MM:SS.mmm PID TID LEVEL TAG : message
    //    Example: 02-10 12:34:23.772  2577  4448 D PowerUI : can't show warning
    // 2. Package variant: MM-DD HH:MM:SS.mmm PACKAGE LEVEL/TAG: message
    //    Example: 02-03 09:34:57.012 com.android.bluetooth I/BtGatt.ScanManager: msg.what = 6
    // Note: There can be spaces before the colon (e.g., "TAG :" or "TAG:")
    // The \s* before ":" trims trailing space from the tag; nothing is trimmed after
    // the colon so leading whitespace in the message is preserved.
    m_regex.setPattern("^(\\d{2}-\\d{2})\\s+(\\d{2}:\\d{2}:\\d{2}\\.\\d{3})\\s+(\\S+)(?:\\s+(\\d+))?\\s+([VDIWEA])[/\\s]+(.+?)\\s*:(.*)$");
}

LogEntry ThreadtimeLogConverter::convert(const QString &line) const
{
    LogEntry entry;
    
    QRegularExpressionMatch match = m_regex.match(line);
    
    if (match.hasMatch()) {
        QString dateStr = match.captured(1); // MM-DD
        entry.time = match.captured(2); // HH:MM:SS.mmm
        QString pidOrPackage = match.captured(3); // Could be PID or package name
        QString possibleTid = match.captured(4); // TID if format is standard, empty if package variant
        entry.level = match.captured(5);
        entry.tag = match.captured(6);
        entry.message = match.captured(7);
        
        // Determine if this is standard format (numeric PID/TID) or package variant
        bool isNumeric = pidOrPackage.toInt() > 0 || pidOrPackage == "0";
        if (isNumeric && !possibleTid.isEmpty()) {
            // Standard format: MM-DD HH:MM:SS.mmm PID TID LEVEL TAG: message
            entry.pid = pidOrPackage;
            entry.tid = possibleTid;
            entry.package = "";
        } else {
            // Package variant: MM-DD HH:MM:SS.mmm PACKAGE LEVEL/TAG: message
            entry.package = pidOrPackage;
            entry.pid = "";
            entry.tid = "";
        }
        
        // Add current year to the date (cached for ~5 minutes to avoid per-line QDateTime cost)
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (m_cachedYear == 0 || nowMs - m_cachedYearStampMs > 5LL * 60 * 1000) {
            m_cachedYear = QDateTime::currentDateTime().date().year();
            m_cachedYearStampMs = nowMs;
        }
        entry.date = QString::number(m_cachedYear) + QLatin1Char('-') + dateStr;
    }
    
    return entry;
}

QString ThreadtimeLogConverter::name() const
{
    return "Threadtime";
}

QString ThreadtimeLogConverter::formatDescription() const
{
    return "Android logcat threadtime format (MM-DD HH:MM:SS.mmm PID TID LEVEL TAG: message)";
}
