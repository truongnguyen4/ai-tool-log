#ifndef CRASHDETECTOR_H
#define CRASHDETECTOR_H

#include <QCoreApplication>
#include <QString>

#include "logentry.h"

/**
 * Spots the lines that announce a crash or an ANR.
 *
 * These are the lines a developer scrolls back for, and they are easy to miss
 * in a fast log — so the ones matched here mark themselves as they arrive.
 * The rules stay narrow on purpose: a false positive marks noise, which costs
 * more than a missed line.
 */
namespace CrashDetector {

enum class Kind : quint8 {
    None,
    JavaCrash,    ///< AndroidRuntime: FATAL EXCEPTION
    NativeCrash,  ///< a fatal signal, or a tombstone header
    Anr,          ///< ActivityManager: ANR in <app>
};

inline Kind classify(const LogEntry &entry)
{
    // The converter keeps the message exactly as logged, leading space and all.
    const QString message = entry.message.trimmed();
    if (message.startsWith(QLatin1String("ANR in ")))
        return Kind::Anr;
    if (entry.tag == QLatin1String("AndroidRuntime")
        && message.contains(QLatin1String("FATAL EXCEPTION")))
        return Kind::JavaCrash;
    if (message.contains(QLatin1String("Fatal signal ")))
        return Kind::NativeCrash;
    if (entry.tag == QLatin1String("DEBUG") && message.startsWith(QLatin1String("*** ***")))
        return Kind::NativeCrash;
    return Kind::None;
}

inline QString describe(Kind kind)
{
    switch (kind) {
    case Kind::JavaCrash:   return QCoreApplication::translate("CrashDetector", "Crash");
    case Kind::NativeCrash: return QCoreApplication::translate("CrashDetector", "Native crash");
    case Kind::Anr:         return QCoreApplication::translate("CrashDetector", "ANR");
    case Kind::None:        break;
    }
    return QString();
}

} // namespace CrashDetector

#endif // CRASHDETECTOR_H
