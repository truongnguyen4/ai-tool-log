#include "adbexecutor.h"

QString AdbProcessResult::errorMessage(const QString &fallback) const
{
    const QString err = standardError.trimmed();
    if (!err.isEmpty())
        return err;
    if (timedOut)
        return fallback.isEmpty() ? QStringLiteral("Command timeout") : fallback;
    if (!started)
        return fallback.isEmpty() ? QStringLiteral("Failed to start adb") : fallback;
    if (exitStatus != QProcess::NormalExit)
        return fallback.isEmpty() ? QStringLiteral("adb crashed") : fallback;
    if (exitCode != 0)
        return QStringLiteral("Command failed with exit code %1").arg(exitCode);
    return fallback;
}

AdbProcessResult AdbExecutor::run(const QString &adbPath,
                                  const QStringList &arguments,
                                  int timeoutMs)
{
    AdbProcessResult result;

    QProcess process;
    process.start(adbPath, arguments);
    if (!process.waitForStarted(3000)) {
        result.processError = process.error();
        result.standardError = process.errorString();
        return result;
    }

    result.started = true;
    if (!process.waitForFinished(timeoutMs)) {
        result.timedOut = true;
        result.processError = process.error();
        process.kill();
        process.waitForFinished(1000);
    }

    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.processError = process.error();
    result.standardOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.standardError = QString::fromUtf8(process.readAllStandardError());
    return result;
}
