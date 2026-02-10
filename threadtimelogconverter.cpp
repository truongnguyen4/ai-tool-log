#include "threadtimelogconverter.h"
#include <QDateTime>

ThreadtimeLogConverter::ThreadtimeLogConverter()
{
    // Regex pattern for threadtime format: MM-DD HH:MM:SS.mmm PID TID LEVEL TAG : message
    // Example: 02-10 12:34:23.772  2577  4448 D PowerUI : can't show warning
    // Note: There can be spaces before the colon (e.g., "TAG :" or "TAG:")
    m_regex.setPattern("^(\\d{2}-\\d{2})\\s+(\\d{2}:\\d{2}:\\d{2}\\.\\d{3})\\s+(\\d+)\\s+(\\d+)\\s+([VDIWEA])\\s+(.+?)\\s*:\\s*(.*)$");
}

LogEntry ThreadtimeLogConverter::convert(const QString &line) const
{
    LogEntry entry;
    
    QRegularExpressionMatch match = m_regex.match(line);
    
    if (match.hasMatch()) {
        QString dateStr = match.captured(1); // MM-DD
        entry.time = match.captured(2); // HH:MM:SS.mmm
        entry.pid = match.captured(3);
        entry.tid = match.captured(4);
        entry.level = match.captured(5);
        entry.tag = match.captured(6).trimmed();
        entry.message = match.captured(7);
        entry.package = ""; // Not available in threadtime format
        
        // Add current year to the date
        int currentYear = QDateTime::currentDateTime().date().year();
        entry.date = QString("%1-%2").arg(currentYear).arg(dateStr);
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
