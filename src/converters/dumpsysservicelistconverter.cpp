#include "dumpsysservicelistconverter.h"

QStringList DumpsysServiceListConverter::convert(const QString &output)
{
    QStringList services;
    for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QString svc = line.trimmed();
        if (!svc.isEmpty() && !svc.startsWith(QLatin1String("Currently")))
            services << svc;
    }
    services.sort(Qt::CaseInsensitive);
    return services;
}
