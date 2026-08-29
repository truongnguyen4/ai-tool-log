#ifndef DMESGLOGCONVERTER_H
#define DMESGLOGCONVERTER_H

#include "ilogconverter.h"
#include <QRegularExpression>

/**
 * Converter for Linux kernel ring-buffer lines (`adb shell dmesg -w`).
 *
 * Format: [<seconds since boot>] [<subsystem>] message
 * Example: [   12.345678] [c0] binder: 1234:1234 transaction failed
 *
 * The subsystem tag is optional; lines without one are tagged "KERNEL".
 * dmesg carries no severity, so every line is reported at Info level.
 *
 * Implementing this as an ILogConverter lets kernel logs travel the same
 * batched ingest path as logcat instead of the per-line path they used to
 * take, which re-evaluated the filter criteria for every single line.
 */
class DmesgLogConverter : public ILogConverter
{
public:
    DmesgLogConverter();
    ~DmesgLogConverter() override = default;

    LogEntry convert(const QString &line) const override;
    QString name() const override;
    QString formatDescription() const override;

private:
    QRegularExpression m_regex;
};

#endif // DMESGLOGCONVERTER_H
