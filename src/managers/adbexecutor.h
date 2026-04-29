#ifndef ADBEXECUTOR_H
#define ADBEXECUTOR_H

#include <QProcess>
#include <QString>
#include <QStringList>

struct AdbProcessResult {
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    QProcess::ProcessError processError = QProcess::UnknownError;
    QString standardOutput;
    QString standardError;
    bool timedOut = false;
    bool started = false;

    bool succeeded() const
    {
        return started && !timedOut && exitStatus == QProcess::NormalExit
               && exitCode == 0 && standardError.trimmed().isEmpty();
    }

    bool completed() const
    {
        return started && !timedOut && exitStatus == QProcess::NormalExit
               && exitCode == 0;
    }

    QString errorMessage(const QString &fallback = QString()) const;
};

class AdbExecutor
{
public:
    static AdbProcessResult run(const QString &adbPath,
                                const QStringList &arguments,
                                int timeoutMs);
};

#endif // ADBEXECUTOR_H
