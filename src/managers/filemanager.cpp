#include "filemanager.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QRegularExpression>

FileManager::FileManager()
    : m_lastLineCount(0)
    , m_lastParsedCount(0)
{
}

QVector<LogEntry> FileManager::readFromFile(const QString &filePath,
                                             const LogConverterPtr &converter,
                                             QString &errorMsg)
{
    QVector<LogEntry> logs;
    m_lastLineCount = 0;
    m_lastParsedCount = 0;
    
    // Validate input
    if (filePath.isEmpty()) {
        errorMsg = "File path is empty";
        return logs;
    }
    
    if (!converter) {
        errorMsg = "Log converter is null";
        return logs;
    }
    
    // Check if file exists
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        errorMsg = QString("File does not exist: %1").arg(filePath);
        return logs;
    }
    
    if (!fileInfo.isFile()) {
        errorMsg = QString("Path is not a file: %1").arg(filePath);
        return logs;
    }
    
    // Open file for reading
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QString("Failed to open file: %1").arg(file.errorString());
        return logs;
    }
    
    // Read and parse lines
    QTextStream in(&file);
    
    while (!in.atEnd()) {
        QString line = in.readLine();
        m_lastLineCount++;
        
        // Skip empty lines
        if (line.trimmed().isEmpty()) {
            continue;
        }
        
        // Skip logcat header lines (e.g., "--------- beginning of system")
        if (line.trimmed().startsWith("---------")) {
            continue;
        }
        
        // Try to parse the line
        LogEntry entry = converter->convert(line);
        
        if (entry.isValid()) {
            logs.append(entry);
            m_lastParsedCount++;
        }
    }
    
    file.close();
    
    // Success
    errorMsg.clear();
    return logs;
}

bool FileManager::saveToFile(const QString &filePath,
                              const QVector<LogEntry> &logs,
                              QString &errorMsg)
{
    // Validate input
    if (filePath.isEmpty()) {
        errorMsg = "File path is empty";
        return false;
    }
    
    // Open file for writing
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        errorMsg = QString("Failed to open file for writing: %1").arg(file.errorString());
        return false;
    }
    
    QTextStream out(&file);
    
    // Write header
    out << "# Log file saved by ToolLogPro\n";
    out << "# Format: MM-DD HH:MM:SS.mmm PID TID LEVEL TAG: message (threadtime format)\n";
    out << "# Total entries: " << logs.size() << "\n";
    out << "\n";
    
    // Write log entries
    for (const LogEntry &entry : logs) {
        out << formatLogEntry(entry) << "\n";
    }
    
    file.close();
    
    // Success
    errorMsg.clear();
    return true;
}

QVector<LogEntry> FileManager::readFromFileAuto(const QString &filePath,
                                                 const QVector<LogConverterPtr> &converters,
                                                 LogConverterPtr &usedConverter,
                                                 QString &errorMsg)
{
    QVector<LogEntry> bestResult;
    LogConverterPtr bestConverter;
    int bestParsedCount = 0;
    int bestLineCount = 0;
    
    // Try each converter
    for (const LogConverterPtr &converter : converters) {
        QString converterError;
        QVector<LogEntry> result = readFromFile(filePath, converter, converterError);
        
        // Check if this converter performed better
        if (m_lastParsedCount > bestParsedCount) {
            bestParsedCount = m_lastParsedCount;
            bestLineCount = m_lastLineCount;
            bestResult = result;
            bestConverter = converter;
        }
        
        // If we got 100% parse rate, use this converter
        if (m_lastLineCount > 0 && m_lastParsedCount == m_lastLineCount) {
            break;
        }
    }
    
    // Check if we found any valid converter
    if (bestParsedCount == 0) {
        errorMsg = "No converter could parse the file successfully";
        usedConverter.reset();
        return QVector<LogEntry>();
    }
    
    // Restore the counts from the best converter
    m_lastParsedCount = bestParsedCount;
    m_lastLineCount = bestLineCount;
    
    // Return best result
    usedConverter = bestConverter;
    errorMsg.clear();
    return bestResult;
}

int FileManager::getLastLineCount() const
{
    return m_lastLineCount;
}

int FileManager::getLastParsedCount() const
{
    return m_lastParsedCount;
}

QString FileManager::formatLogEntry(const LogEntry &entry) const
{
    // Format: MM-DD HH:MM:SS.mmm PID TID LEVEL TAG: message
    // This matches the threadtime format that converters can parse
    QString formatted;
    
    // Extract MM-DD from date (handles both YYYY-MM-DD and MM-DD formats)
    QString dateStr;
    if (!entry.date.isEmpty()) {
        if (entry.date.contains('-')) {
            QStringList dateParts = entry.date.split('-');
            if (dateParts.size() == 3) {
                // Format: YYYY-MM-DD, extract MM-DD
                dateStr = QString("%1-%2").arg(dateParts[1]).arg(dateParts[2]);
            } else if (dateParts.size() == 2) {
                // Already in MM-DD format
                dateStr = entry.date;
            }
        } else {
            dateStr = entry.date;
        }
    }
    
    if (!dateStr.isEmpty()) {
        formatted += dateStr;
    } else {
        formatted += "01-01";
    }
    
    formatted += " ";
    
    // Add time
    if (!entry.time.isEmpty()) {
        formatted += entry.time;
    } else {
        formatted += "00:00:00.000";
    }
    
    formatted += "  ";
    
    if (!entry.pid.isEmpty()) {
        formatted += QString("%1").arg(entry.pid, 5);
    } else {
        formatted += "    ?";
    }
    
    formatted += "  ";
    
    if (!entry.tid.isEmpty()) {
        formatted += QString("%1").arg(entry.tid, 5);
    } else {
        formatted += "    ?";
    }
    
    formatted += " ";
    formatted += entry.level.isEmpty() ? "?" : entry.level;
    formatted += " ";
    formatted += entry.tag.isEmpty() ? "Unknown" : entry.tag;
    formatted += ": ";
    formatted += entry.message;
    
    return formatted;
}
