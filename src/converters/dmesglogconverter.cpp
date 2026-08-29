#include "dmesglogconverter.h"

namespace {
constexpr auto kDefaultTag   = "KERNEL";
constexpr auto kDefaultLevel = "I";
} // namespace

DmesgLogConverter::DmesgLogConverter()
    : m_regex(QStringLiteral(R"(^\[\s*([\d.]+)\]\s*(?:\[([^\]]*)\])?\s*(.*)$)"))
{
    m_regex.optimize();
}

LogEntry DmesgLogConverter::convert(const QString &line) const
{
    LogEntry entry;

    const QRegularExpressionMatch match = m_regex.match(line);
    if (!match.hasMatch())
        return entry;

    entry.time    = match.captured(1).trimmed();
    entry.tag     = match.captured(2).trimmed();
    entry.message = match.captured(3).trimmed();
    if (entry.tag.isEmpty())
        entry.tag = QLatin1String(kDefaultTag);
    entry.level = QLatin1String(kDefaultLevel);

    return entry;
}

QString DmesgLogConverter::name() const
{
    return QStringLiteral("Kernel (dmesg)");
}

QString DmesgLogConverter::formatDescription() const
{
    return QStringLiteral("Linux kernel ring buffer ([timestamp] [tag] message)");
}
