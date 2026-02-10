#include "threadtimelogconverter.h"

ThreadtimeLogConverter::ThreadtimeLogConverter()
{
    // Regex pattern for threadtime format: MM-DD HH:MM:SS.mmm PID TID LEVEL TAG : message
    // Example: 02-10 12:34:23.772  2577  4448 D PowerUI : can't show warning
    // Note: There can be spaces before the colon (e.g., "TAG :" or "TAG:")
    m_regex.setPattern("^(\\d{2}-\\d{2}\\s+\\d{2}:\\d{2}:\\d{2}\\.\\d{3})\\s+(\\d+)\\s+(\\d+)\\s+([VDIWEA])\\s+(.+?)\\s*:\\s*(.*)$");
}

LogEntry ThreadtimeLogConverter::convert(const QString &line) const
{
    LogEntry entry;
    
    QRegularExpressionMatch match = m_regex.match(line);
    
    if (match.hasMatch()) {
        entry.time = match.captured(1);
        entry.pid = match.captured(2);
        entry.tid = match.captured(3);
        entry.level = match.captured(4);
        entry.tag = match.captured(5).trimmed();
        entry.message = match.captured(6);
        entry.package = ""; // Not available in threadtime format
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
