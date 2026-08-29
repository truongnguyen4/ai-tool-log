#include "brieflogconverter.h"
#include <QDateTime>

namespace {
/**
 * How long a formatted timestamp is reused, in milliseconds.
 *
 * The brief format carries no timestamp, so one is synthesised. Formatting a
 * QDateTime twice per line dominated the cost of loading a brief-format file,
 * and millisecond precision is meaningless for a value we invented anyway.
 */
constexpr qint64 kTimestampCacheMs = 1000;
} // namespace

BriefLogConverter::BriefLogConverter()
{
    // Format: LEVEL/TAG(PID): message
    // Leading whitespace in the message is intentionally preserved.
    m_regex.setPattern(QStringLiteral(R"(^([VDIWEA])/(.+?)\((\d+)\):(.*)$)"));
    m_regex.optimize();
}

LogEntry BriefLogConverter::convert(const QString &line) const
{
    LogEntry entry;

    const QRegularExpressionMatch match = m_regex.match(line);
    if (!match.hasMatch())
        return entry;

    entry.level   = match.captured(1);
    entry.tag     = match.captured(2);
    entry.pid     = match.captured(3);
    entry.message = match.captured(4);

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_cachedStampMs == 0 || nowMs - m_cachedStampMs > kTimestampCacheMs) {
        const QDateTime now = QDateTime::currentDateTime();
        m_cachedDate   = now.toString(QStringLiteral("yyyy-MM-dd"));
        m_cachedTime   = now.toString(QStringLiteral("hh:mm:ss.zzz"));
        m_cachedStampMs = nowMs;
    }
    entry.date = m_cachedDate;
    entry.time = m_cachedTime;

    return entry;
}

QString BriefLogConverter::name() const
{
    return QStringLiteral("Brief");
}

QString BriefLogConverter::formatDescription() const
{
    return QStringLiteral("Android logcat brief format (LEVEL/TAG(PID): message)");
}
