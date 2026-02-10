#include "filemanager.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QRegularExpression>
#include <cstring>  // memchr

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
    
    // Open file for reading (binary – we handle newlines ourselves)
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = QString("Failed to open file: %1").arg(file.errorString());
        return logs;
    }

    const qint64 fileSize = file.size();
    if (fileSize == 0) {
        file.close();
        errorMsg.clear();
        return logs;
    }

    // Pre-size the output vector: average logcat line ≈ 80 bytes.
    logs.reserve(static_cast<int>(qMin<qint64>(fileSize / 80 + 1, 3'000'000)));

    // -----------------------------------------------------------------------
    // Fast path: memory-map the file so we never copy bytes into a QByteArray.
    // memchr() scans for '\n' using SIMD on modern glibc — far faster than
    // QTextStream::readLine() which allocates a new QString per line.
    // -----------------------------------------------------------------------
    const uchar *mapped = file.map(0, fileSize);
    if (mapped) {
        const char *data      = reinterpret_cast<const char *>(mapped);
        const char *dataEnd   = data + fileSize;
        const char *lineStart = data;

        while (lineStart < dataEnd) {
            const char *nl =
                static_cast<const char *>(memchr(lineStart, '\n', dataEnd - lineStart));
            const char *lineEnd = nl ? nl : dataEnd;

            int lineLen = static_cast<int>(lineEnd - lineStart);
            if (lineLen > 0 && lineStart[lineLen - 1] == '\r')
                --lineLen; // strip CRLF

            if (lineLen > 0) {
                // Fast-reject the "--------- beginning of ..." separator
                if (lineLen < 9 || memcmp(lineStart, "---------", 9) != 0) {
                    ++m_lastLineCount;
                    // fromUtf8 with a length – no NUL terminator needed
                    QString line = QString::fromUtf8(lineStart, lineLen);
                    LogEntry entry = converter->convert(line);
                    if (entry.isValid()) {
                        logs.append(std::move(entry));
                        ++m_lastParsedCount;
                    }
                }
            }

            lineStart = nl ? nl + 1 : dataEnd;
        }

        file.unmap(const_cast<uchar *>(mapped));
    } else {
        // -----------------------------------------------------------------------
        // Fallback (mmap refused – e.g. very large file on 32-bit): chunked read.
        // Still faster than QTextStream: we handle newline splitting manually and
        // only call QString::fromUtf8 on individual lines.
        // -----------------------------------------------------------------------
        constexpr int CHUNK = 4 * 1024 * 1024; // 4 MB
        QByteArray leftover;
        leftover.reserve(512);

        while (!file.atEnd()) {
            QByteArray chunk = file.read(CHUNK);
            if (!leftover.isEmpty()) {
                chunk.prepend(leftover);
                leftover.clear();
            }

            const char *data = chunk.constData();
            const int   n    = chunk.size();
            int lineStart = 0;

            for (int i = 0; i < n; ++i) {
                if (data[i] == '\n') {
                    int lineEnd = i;
                    if (lineEnd > lineStart && data[lineEnd - 1] == '\r')
                        --lineEnd;
                    const int lineLen = lineEnd - lineStart;
                    if (lineLen > 0) {
                        if (lineLen < 9 || memcmp(data + lineStart, "---------", 9) != 0) {
                            ++m_lastLineCount;
                            QString line = QString::fromUtf8(data + lineStart, lineLen);
                            LogEntry entry = converter->convert(line);
                            if (entry.isValid()) {
                                logs.append(std::move(entry));
                                ++m_lastParsedCount;
                            }
                        }
                    }
                    lineStart = i + 1;
                }
            }
            if (lineStart < n)
                leftover = chunk.mid(lineStart);
        }

        // Handle final line without trailing newline
        if (!leftover.isEmpty()) {
            int lineLen = leftover.size();
            if (lineLen > 0 && leftover[lineLen - 1] == '\r') --lineLen;
            if (lineLen > 0) {
                const char *d = leftover.constData();
                if (lineLen < 9 || memcmp(d, "---------", 9) != 0) {
                    ++m_lastLineCount;
                    QString line = QString::fromUtf8(d, lineLen);
                    LogEntry entry = converter->convert(line);
                    if (entry.isValid()) {
                        logs.append(std::move(entry));
                        ++m_lastParsedCount;
                    }
                }
            }
        }
    }

    file.close();

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
    // -----------------------------------------------------------------------
    // Phase 1: format detection – sample the first N non-header lines so we
    // can pick the best converter without reading the entire (potentially
    // hundreds-of-megabytes) file for every candidate converter.
    // -----------------------------------------------------------------------
    constexpr int SAMPLE_LINES = 100;

    QFile sampleFile(filePath);
    if (!sampleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = QString("Failed to open file: %1").arg(sampleFile.errorString());
        usedConverter.reset();
        return {};
    }

    QVector<QString> samples;
    samples.reserve(SAMPLE_LINES);
    {
        QTextStream in(&sampleFile);
        while (!in.atEnd() && samples.size() < SAMPLE_LINES) {
            const QString line = in.readLine();
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith("---------"))
                continue;
            samples.append(line);
        }
    }
    sampleFile.close();

    // Score each converter on the sample lines
    LogConverterPtr bestConverter;
    int bestScore = -1;
    for (const LogConverterPtr &conv : converters) {
        int score = 0;
        for (const QString &line : samples)
            if (conv->convert(line).isValid()) ++score;
        if (score > bestScore) {
            bestScore = score;
            bestConverter = conv;
        }
        if (bestScore == samples.size()) break;   // perfect score – no need to continue
    }

    if (bestScore <= 0 || !bestConverter) {
        errorMsg = "No converter could parse the file successfully";
        usedConverter.reset();
        return {};
    }

    // -----------------------------------------------------------------------
    // Phase 2: full single-pass parse with the winning converter.
    // -----------------------------------------------------------------------
    QVector<LogEntry> result = readFromFile(filePath, bestConverter, errorMsg);
    usedConverter = bestConverter;
    return result;
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
