#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QString>
#include <QVector>
#include "ilogconverter.h"

/**
 * FileManager handles reading and writing log files
 * Uses ILogConverter for parsing log lines
 * Completely independent of UI and other components
 */
class FileManager
{
public:
    FileManager();
    ~FileManager() = default;
    
    /**
     * Read logs from a file using the specified converter
     * @param filePath Path to the log file
     * @param converter Log converter to use for parsing
     * @param errorMsg Output parameter for error messages
     * @return Vector of parsed log entries, empty if error occurred
     */
    QVector<LogEntry> readFromFile(const QString &filePath, 
                                    const LogConverterPtr &converter,
                                    QString &errorMsg);
    
    /**
     * Save logs to a file
     * @param filePath Path where to save the log file
     * @param logs Vector of log entries to save
     * @param errorMsg Output parameter for error messages
     * @return true if successful, false otherwise
     */
    bool saveToFile(const QString &filePath,
                    const QVector<LogEntry> &logs,
                    QString &errorMsg);
    
    /**
     * Read logs from file and try multiple converters
     * Automatically detects the best converter based on successful parsing
     * @param filePath Path to the log file
     * @param converters List of converters to try
     * @param usedConverter Output parameter for the converter that worked
     * @param errorMsg Output parameter for error messages
     * @return Vector of parsed log entries
     */
    QVector<LogEntry> readFromFileAuto(const QString &filePath,
                                        const QVector<LogConverterPtr> &converters,
                                        LogConverterPtr &usedConverter,
                                        QString &errorMsg);
    
    /**
     * Get the number of lines read in last operation
     * @return Line count
     */
    int getLastLineCount() const;
    
    /**
     * Get the number of successfully parsed entries in last operation
     * @return Parsed entry count
     */
    int getLastParsedCount() const;
    
private:
    int m_lastLineCount;
    int m_lastParsedCount;
    
    QString formatLogEntry(const LogEntry &entry) const;
};

#endif // FILEMANAGER_H
