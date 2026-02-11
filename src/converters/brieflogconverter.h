#ifndef BRIEFLOGCONVERTER_H
#define BRIEFLOGCONVERTER_H

#include "ilogconverter.h"
#include <QRegularExpression>

/**
 * Converter for Android logcat brief format
 * Format: LEVEL/TAG(PID): message
 * Example: I/MyTag(1234): Log message here
 */
class BriefLogConverter : public ILogConverter
{
public:
    BriefLogConverter();
    ~BriefLogConverter() override = default;
    
    LogEntry convert(const QString &line) const override;
    QString name() const override;
    QString formatDescription() const override;
    
private:
    QRegularExpression m_regex;
};

#endif // BRIEFLOGCONVERTER_H
