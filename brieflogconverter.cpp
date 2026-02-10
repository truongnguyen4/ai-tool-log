#include "brieflogconverter.h"
#include <QDateTime>

BriefLogConverter::BriefLogConverter()
{
    // Regex pattern for brief format: LEVEL/TAG(PID): message
    // Example: I/MyTag(1234): Log message here
    m_regex.setPattern("^([VDIWEA])/(.+?)\\((\\d+)\\):\\s*(.*)$");
}

LogEntry BriefLogConverter::convert(const QString &line) const
{
    LogEntry entry;
    
    QRegularExpressionMatch match = m_regex.match(line);
    
    if (match.hasMatch()) {
        entry.level = match.captured(1);
        entry.tag = match.captured(2).trimmed();
        entry.pid = match.captured(3);
        entry.message = match.captured(4);
        
        // Brief format doesn't have time or TID, generate current time
        entry.time = QDateTime::currentDateTime().toString("MM-dd hh:mm:ss.zzz");
        entry.tid = "";
        entry.package = "";
    }
    
    return entry;
}

QString BriefLogConverter::name() const
{
    return "Brief";
}

QString BriefLogConverter::formatDescription() const
{
    return "Android logcat brief format (LEVEL/TAG(PID): message)";
}
