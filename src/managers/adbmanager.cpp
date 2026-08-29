#include "adbmanager.h"
#include "adbcommand.h"
#include "adbexecutor.h"
#include "devicelistconverter.h"
#include "dumpsysservicelistconverter.h"
#include "propertieslistconverter.h"
#include "settingslistconverter.h"
#include <QCoreApplication>
#include <QDebug>
#include <QRegularExpression>
#include <QMetaObject>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <utility>

namespace {

constexpr int kPropertyListTimeoutMs  = 20000;
constexpr int kPropertyWriteTimeoutMs = 30000;

/** Pause before asking again for a capture device that did not answer. */
constexpr int    kCaptureRetryDelayMs  = 1000;
/** A stream shorter than this ended with its device still going away. */
constexpr qint64 kBriefCaptureStreamMs = 3000;

/** "MM-DD hh:mm:ss.mmm" leading a threadtime line, or an empty string. */
QString threadtimeStamp(const QString &line)
{
    constexpr int kStampLength = 18;
    if (line.size() < kStampLength || line.at(2) != QLatin1Char('-') || line.at(5) != QLatin1Char(' ')
        || line.at(8) != QLatin1Char(':') || line.at(14) != QLatin1Char('.'))
        return QString();
    return line.left(kStampLength);
}

/** Seconds since boot leading a kernel line: "[ 64551.936242] ...". */
QString kernelStamp(const QString &line)
{
    static const QRegularExpression pattern(QStringLiteral("^\\[\\s*([\\d.]+)\\]"));
    const QRegularExpressionMatch match = pattern.match(line);
    return match.hasMatch() ? match.captured(1) : QString();
}

/**
 * The time at the start of @p line as the log spells it (@p text) and as a
 * key that sorts in time order. Kernel seconds are zero-padded to a fixed
 * width, so comparing the keys as strings compares them as numbers.
 */
QString captureStampKey(AdbManager::CaptureKind kind, const QString &line, QString *text)
{
    const QString stamp = kind == AdbManager::CaptureKind::Logcat ? threadtimeStamp(line)
                                                                  : kernelStamp(line);
    if (text)
        *text = stamp;
    if (stamp.isEmpty() || kind == AdbManager::CaptureKind::Logcat)
        return stamp;
    return QStringLiteral("%1").arg(stamp.toDouble(), 20, 'f', 6, QLatin1Char('0'));
}

/** Why the kernel log stopped; dmesg on Android needs root. */
QString kernelFailureMessage(const QString &output, int exitCode)
{
    const QString lower = output.toLower();
    if (lower.contains(QLatin1String("permission denied"))
        || lower.contains(QLatin1String("operation not permitted"))
        || lower.contains(QLatin1String("not permitted"))
        || lower.contains(QLatin1String("eperm"))) {
        return QCoreApplication::translate(
            "AdbManager", "Root access required. Enable root mode on the device "
                          "(Developer options → Root access) and try again.");
    }
    if (!output.isEmpty())
        return output;
    return QCoreApplication::translate(
               "AdbManager", "dmesg exited with code %1. Root access may be required.")
        .arg(exitCode);
}

/** Run one `set` or `reset` command and read what it reported. */
PropertyWriteResult runPropertyWrite(const QString &adbPath, const QString &deviceId,
                                     const QStringList &args)
{
    const AdbProcessResult result =
        AdbExecutor::run(adbPath, AdbCommand::configurationManager(deviceId, args),
                         kPropertyWriteTimeoutMs);
    PropertyWriteResult parsed;
    if (!result.started || result.timedOut) {
        parsed.error = result.errorMessage(QStringLiteral("Timed out writing properties"));
        return parsed;
    }
    // "Properties not found" and exceptions can arrive on either stream.
    parsed = ConfigurationManagerOutput::parseWriteOutput(
        result.standardOutput + QLatin1Char('\n') + result.standardError);
    if (!result.succeeded() && parsed.succeeded() && parsed.done.isEmpty())
        parsed.error = QStringLiteral("configuration_manager %1 failed").arg(args.value(0));
    return parsed;
}

} // namespace

AdbManager::AdbManager(QObject *parent)
    : QObject(parent)
    , m_adbPath("adb")
    , m_deviceDetectionTimer(new QTimer(this))
{
    // Set up device detection timer (check every 2 seconds)
    m_deviceDetectionTimer->setInterval(2000);
    connect(m_deviceDetectionTimer, &QTimer::timeout, this, &AdbManager::detectDevices);
    m_deviceDetectionTimer->start();
    
    // Initial device detection
    detectDevices();
}

AdbManager::~AdbManager()
{
    // Both capture processes are children of this object; kill them explicitly
    // so neither outlives the app as an orphaned `adb logcat` / `adb dmesg`.
    stopLogcat();
    stopDmesg();
    if (m_deviceDetectionTimer)
        m_deviceDetectionTimer->stop();
}

AdbManager& AdbManager::instance()
{
    static AdbManager instance;
    return instance;
}

QList<AdbDevice> AdbManager::getConnectedDevices()
{
    return m_connectedDevices;
}

void AdbManager::detectDevices()
{
    // Async: never block the UI thread. If a previous detect is still running,
    // skip this tick (the next 2s timer fire will retry).
    if (m_detectInFlight)
        return;

    m_detectInFlight = true;
    const QString adbPath = m_adbPath;
    (void)QtConcurrent::run([this, adbPath]() {
        const AdbProcessResult result =
            AdbExecutor::run(adbPath, AdbCommand::listDevices(), 5000);

        QMetaObject::invokeMethod(this, [this, result]() {
            if (!result.started || result.timedOut
                || result.processError != QProcess::UnknownError) {
                const QString errMsg = result.errorMessage(
                    QStringLiteral("adb devices failed"));
                qWarning() << "[AdbManager] detectDevices:" << errMsg;
                emit errorOccurred(errMsg);
            } else {
                if (!result.standardError.trimmed().isEmpty())
                    qWarning() << "[AdbManager] detectDevices: stderr ="
                               << result.standardError.trimmed();
                parseDeviceList(result.standardOutput);
            }
            m_detectInFlight = false;
        }, Qt::QueuedConnection);
    });
}

void AdbManager::parseDeviceList(const QString &output)
{
    const QList<AdbDevice> newDevices = DeviceListConverter::convert(output);

    // Compare identity *and* state: a device going offline keeps its serial,
    // so an id-only comparison left the UI showing it as still connected.
    bool changed = newDevices.size() != m_connectedDevices.size();
    for (int i = 0; !changed && i < newDevices.size(); ++i) {
        const AdbDevice &fresh   = newDevices.at(i);
        const AdbDevice &current = m_connectedDevices.at(i);
        changed = fresh.id != current.id
                  || fresh.isOnline != current.isOnline
                  || fresh.name != current.name;
    }

    if (changed) {
        m_connectedDevices = newDevices;

        if (!m_currentDeviceId.isEmpty()) {
            const bool found = std::any_of(newDevices.cbegin(), newDevices.cend(),
                [&](const AdbDevice &d) { return d.id == m_currentDeviceId; });
            if (!found) m_currentDeviceId.clear();
        }

        emit devicesChanged(m_connectedDevices);
    }
}

QString AdbManager::getCurrentDeviceId() const
{
    return m_currentDeviceId;
}

void AdbManager::setCurrentDeviceId(const QString &deviceId)
{
    if (m_currentDeviceId != deviceId) {
        m_currentDeviceId = deviceId;
        qDebug() << "AdbManager: Current device set to" << deviceId;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Capture sessions — logcat and the kernel log
//
//   waiting   adb -s <serial> wait-for-device shell cat .../boot_id
//             returns the moment adbd on the device answers — right after a
//             reboot, long before the boot completes — with the boot's id
//   streaming adb -s <serial> logcat -v threadtime [-T <last line's time>]
//             adb -s <serial> shell dmesg -w
//
// When a stream ends and reboots are followed, the session waits again. A new
// boot id means the device rebooted: the log is read from the start, which
// holds the boot's earliest lines. The same id means only the connection
// dropped, so what was already shown is skipped — logcat is asked to start
// from the last line with -T, while dmesg, which always replays its whole ring
// buffer, is filtered by the kernel timestamps already seen.
// ─────────────────────────────────────────────────────────────────────────────

bool AdbManager::startLogcat(const QString &serial, bool followReboots)
{
    return startCapture(m_logcat, serial, followReboots);
}

bool AdbManager::startDmesg(const QString &serial, bool followReboots)
{
    return startCapture(m_kernel, serial, followReboots);
}

void AdbManager::stopLogcat() { stopCapture(m_logcat); }
void AdbManager::stopDmesg()  { stopCapture(m_kernel); }

bool AdbManager::isLogcatRunning() const { return m_logcat.running; }
bool AdbManager::isDmesgRunning()  const { return m_kernel.running; }
bool AdbManager::isLogcatWaiting() const { return m_logcat.running && m_logcat.waiting; }
bool AdbManager::isDmesgWaiting()  const { return m_kernel.running && m_kernel.waiting; }
QString AdbManager::logcatSerial() const { return m_logcat.serial; }
QString AdbManager::dmesgSerial()  const { return m_kernel.serial; }

void AdbManager::setCaptureFollowReboots(bool follow)
{
    m_logcat.followReboots = follow;
    m_kernel.followReboots = follow;
}

QString AdbManager::captureStamp(CaptureKind kind) const
{
    return kind == CaptureKind::Logcat ? m_logcat.lastStampText : m_kernel.lastStampText;
}

bool AdbManager::startCapture(CaptureSession &session, const QString &serial, bool followReboots)
{
    if (serial.isEmpty())
        return false;
    stopCapture(session);

    session.serial        = serial;
    session.followReboots = followReboots;
    session.bootId.clear();
    session.lastStamp.clear();
    session.lastStampText.clear();
    session.lastStampLines.clear();
    session.streamCount = 0;
    session.skipResumed = false;
    session.userStopped = false;
    session.running     = true;
    if (session.kind == CaptureKind::Logcat)
        emit logcatStarted();
    else
        emit dmesgStarted();

    waitForCaptureDevice(session, false);
    return session.running;   // false when adb itself could not be started
}

void AdbManager::stopCapture(CaptureSession &session)
{
    if (session.retryTimer)
        session.retryTimer->stop();
    session.userStopped = true;   // a stop of our own is not a failure
    for (QProcess **slot : {&session.waitProcess, &session.streamProcess}) {
        QProcess *process = std::exchange(*slot, nullptr);
        if (!process)
            continue;
        QObject::disconnect(process, nullptr, this, nullptr);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(1000);
        }
        process->deleteLater();
    }
    endCaptureSession(session);
    session.userStopped = false;
}

void AdbManager::endCaptureSession(CaptureSession &session)
{
    if (session.retryTimer)
        session.retryTimer->stop();
    session.waiting = false;
    if (!std::exchange(session.running, false))
        return;
    if (session.kind == CaptureKind::Logcat)
        emit logcatStopped();
    else
        emit dmesgStopped();
}

void AdbManager::waitForCaptureDevice(CaptureSession &session, bool reconnecting)
{
    if (!session.running)
        return;
    if (!session.waiting) {
        session.waiting = true;
        if (session.kind == CaptureKind::Logcat)
            emit logcatWaitingForDevice(session.serial, reconnecting);
        else
            emit dmesgWaitingForDevice(session.serial, reconnecting);
    }
    runCaptureWait(session);
}

void AdbManager::scheduleCaptureRetry(CaptureSession &session)
{
    if (!session.retryTimer) {
        session.retryTimer = new QTimer(this);
        session.retryTimer->setSingleShot(true);
        session.retryTimer->setInterval(kCaptureRetryDelayMs);
        connect(session.retryTimer, &QTimer::timeout, this,
                [this, &session]() { runCaptureWait(session); });
    }
    session.retryTimer->start();
}

void AdbManager::runCaptureWait(CaptureSession &session)
{
    if (!session.running || session.waitProcess || session.streamProcess)
        return;

    auto *process = new QProcess(this);
    session.waitProcess = process;
    connect(process, &QProcess::finished, this,
            [this, &session, process](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();
        if (session.waitProcess != process)
            return;
        session.waitProcess = nullptr;

        const QString output = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        const QString errors = QString::fromUtf8(process->readAllStandardError());
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            const bool looksLikeId = !output.isEmpty() && !output.contains(QLatin1Char(' '));
            startCaptureStream(session, looksLikeId ? output : QString());
            return;
        }
        // Online, but the boot id cannot be read: the log still can be.
        const QString answer = output + errors;
        if (answer.contains(QLatin1String("No such file"))
            || answer.contains(QLatin1String("Permission denied"))) {
            startCaptureStream(session, QString());
            return;
        }
        // It went away again before answering — typical while it shuts down
        // for a reboot. Give it a moment, then wait again.
        scheduleCaptureRetry(session);
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, &session, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || session.waitProcess != process)
            return;
        session.waitProcess = nullptr;
        process->deleteLater();
        emit errorOccurred(QStringLiteral("Could not run adb: %1").arg(process->errorString()));
        endCaptureSession(session);
    });
    process->start(m_adbPath, AdbCommand::waitForBootId(session.serial));
}

void AdbManager::startCaptureStream(CaptureSession &session, const QString &bootId)
{
    const bool resumed = session.streamCount > 0;
    const bool newBoot = !resumed || bootId.isEmpty() || bootId != session.bootId;
    if (newBoot) {
        session.lastStamp.clear();
        session.lastStampText.clear();
        session.lastStampLines.clear();
    }
    // logcat can be told where to start again; dmesg cannot, so its replay is
    // dropped line by line in handleCaptureLine().
    const QString since = (!newBoot && session.kind == CaptureKind::Logcat)
                              ? session.lastStampText : QString();
    session.skipResumed = !newBoot && !session.lastStamp.isEmpty();
    session.bootId = bootId;
    session.linesThisStream = 0;
    ++session.streamCount;

    auto *process = new QProcess(this);
    session.streamProcess = process;
    if (session.kind == CaptureKind::Kernel)
        process->setProcessChannelMode(QProcess::MergedChannels);   // dmesg reports errors on stderr

    connect(process, &QProcess::readyReadStandardOutput, this, [this, &session, process]() {
        while (process->canReadLine())
            handleCaptureLine(session, QString::fromUtf8(process->readLine()).trimmed());
    });
    connect(process, &QProcess::finished, this,
            [this, &session, process](int exitCode, QProcess::ExitStatus) {
        process->deleteLater();
        if (session.streamProcess != process)
            return;
        while (process->canReadLine())
            handleCaptureLine(session, QString::fromUtf8(process->readLine()).trimmed());
        // An error message carries no newline, so it is still unread here.
        const QString remaining = QString::fromLocal8Bit(process->readAll()).trimmed();
        session.streamProcess = nullptr;
        if (!session.running)
            return;

        // A kernel stream that ended without a single line never worked —
        // dmesg needs root. Waiting for the device again would not help.
        if (session.kind == CaptureKind::Kernel && !session.userStopped && exitCode != 0
            && session.linesThisStream == 0) {
            emit dmesgFailed(kernelFailureMessage(remaining, exitCode));
            endCaptureSession(session);
            return;
        }
        if (!session.followReboots) {
            endCaptureSession(session);
            return;
        }

        const bool brief = session.streamClock.elapsed() < kBriefCaptureStreamMs;
        session.waiting = true;
        if (session.kind == CaptureKind::Logcat)
            emit logcatWaitingForDevice(session.serial, true);
        else
            emit dmesgWaitingForDevice(session.serial, true);
        // A stream that ended at once usually means the device is still going
        // down; asking for it again straight away would only find it half gone.
        if (brief)
            scheduleCaptureRetry(session);
        else
            runCaptureWait(session);
    });
    connect(process, &QProcess::errorOccurred, this,
            [this, &session, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || session.streamProcess != process)
            return;
        session.streamProcess = nullptr;
        process->deleteLater();
        emit errorOccurred(QStringLiteral("Could not run adb: %1").arg(process->errorString()));
        endCaptureSession(session);
    });

    session.waiting = false;
    session.streamClock.start();
    if (session.kind == CaptureKind::Logcat)
        emit logcatStreaming(session.serial, resumed, newBoot);
    else
        emit dmesgStreaming(session.serial, resumed, newBoot);
    process->start(m_adbPath, session.kind == CaptureKind::Logcat
                                  ? AdbCommand::startLogcat(session.serial, since)
                                  : AdbCommand::startDmesg(session.serial));
}

void AdbManager::handleCaptureLine(CaptureSession &session, const QString &line)
{
    if (line.isEmpty())
        return;

    QString stampText;
    const QString stamp = captureStampKey(session.kind, line, &stampText);
    if (stamp.isEmpty()) {
        // Banners a resumed logcat replays ("--------- beginning of main").
        if (session.skipResumed)
            return;
    } else {
        // A resumed stream repeats what was shown before, up to and including
        // the last line's time.
        if (session.skipResumed) {
            if (stamp < session.lastStamp
                || (stamp == session.lastStamp && session.lastStampLines.contains(line)))
                return;
            session.skipResumed = false;
        }
        if (stamp != session.lastStamp) {
            session.lastStamp     = stamp;
            session.lastStampText = stampText;
            session.lastStampLines.clear();
        }
        session.lastStampLines.insert(line);
    }

    ++session.linesThisStream;
    if (session.kind == CaptureKind::Logcat)
        emit logcatLineReceived(line);
    else
        emit dmesgLineReceived(line);
}

QString AdbManager::getAdbPath() const
{
    return m_adbPath;
}

void AdbManager::setAdbPath(const QString &path)
{
    m_adbPath = path;
}

void AdbManager::fetchSettings(const QString &deviceId)
{
    const QString adbPath = m_adbPath;

    // Issue #7: run in worker thread so the UI is never blocked
    (void)QtConcurrent::run([this, deviceId, adbPath]() {
        QVector<SettingEntry> settings;
        int lineNum = 1;

        const QStringList namespaces = {"global", "system", "secure"};
        for (const QString &ns : namespaces) {
            const AdbProcessResult result =
                AdbExecutor::run(adbPath, AdbCommand::listSettings(deviceId, ns), 5000);
            if (!result.completed()) {
                emit errorOccurred(QString("Failed to fetch %1 settings: %2")
                                       .arg(ns, result.errorMessage()));
                continue;
            }

            settings.append(SettingsListConverter::convert(
                result.standardOutput, ns, &lineNum));
        }

        emit settingsFetched(settings);
    });
}

void AdbManager::fetchProperties(const QString &deviceId)
{
    const QString adbPath = m_adbPath;

    // Issue #7: run in worker thread so the UI is never blocked
    (void)QtConcurrent::run([this, deviceId, adbPath]() {
        const AdbProcessResult result =
            AdbExecutor::run(adbPath, AdbCommand::listProperties(deviceId), 5000);
        if (!result.completed()) {
            emit errorOccurred("Failed to fetch system properties: " + result.errorMessage());
            // Emit empty so listeners clear stale data from a previous device.
            emit propertiesFetched({});
            return;
        }

        emit propertiesFetched(PropertiesListConverter::convert(result.standardOutput));
    });
}

// ---------------------------------------------------------------------------
// Async save operations  (Issue #7: non-blocking set+verify)
// ---------------------------------------------------------------------------

void AdbManager::saveSettingAsync(int row, const QString &deviceId,
                                   const QString &group, const QString &setting,
                                   const QString &newValue)
{
    const QString adbPath = m_adbPath;

    (void)QtConcurrent::run([this, row, deviceId, group, setting, newValue, adbPath]() {
        const QString ns = group.toLower();

        const AdbProcessResult setResult =
            AdbExecutor::run(adbPath, AdbCommand::putSetting(deviceId, ns, setting, newValue), 5000);
        if (!setResult.succeeded()) {
            emit settingSaveResult(row, false, group, setting, newValue, {},
                                   setResult.errorMessage("Timeout while setting value"));
            return;
        }

        const AdbProcessResult verifyResult =
            AdbExecutor::run(adbPath, AdbCommand::getSetting(deviceId, ns, setting), 3000);
        emit settingSaveResult(row, true, group, setting, newValue,
                               verifyResult.standardOutput.trimmed(), {});
    });
}

void AdbManager::savePropertyAsync(int row, const QString &deviceId,
                                    const QString &property, const QString &newValue)
{
    const QString adbPath = m_adbPath;

    (void)QtConcurrent::run([this, row, deviceId, property, newValue, adbPath]() {
        const AdbProcessResult setResult =
            AdbExecutor::run(adbPath, AdbCommand::setProperty(deviceId, property, newValue), 5000);
        if (!setResult.succeeded()) {
            emit propertySaveResult(row, false, property, newValue, {},
                                    setResult.errorMessage("Timeout while setting property"));
            return;
        }

        const AdbProcessResult verifyResult =
            AdbExecutor::run(adbPath, AdbCommand::getProperty(deviceId, property), 3000);
        emit propertySaveResult(row, true, property, newValue,
                                verifyResult.standardOutput.trimmed(), {});
    });
}

void AdbManager::fetchPropertyDefinitions(const QString &deviceId)
{
    const QString adbPath = m_adbPath;

    (void)QtConcurrent::run([this, deviceId, adbPath]() {
        const AdbProcessResult result =
            AdbExecutor::run(adbPath, AdbCommand::listPropertyDefinitions(deviceId),
                             kPropertyListTimeoutMs);
        QString error;
        QVector<PropertyDefinition> definitions;
        if (!result.started || result.timedOut) {
            error = result.errorMessage(QStringLiteral("Timed out reading the property list"));
        } else {
            definitions = ConfigurationManagerOutput::parseList(result.standardOutput.toUtf8(), &error);
            const QString stderrText = result.standardError.trimmed();
            if (!error.isEmpty() && !stderrText.isEmpty())
                error += QLatin1Char('\n') + stderrText;
        }
        emit propertyDefinitionsFetched(deviceId, definitions, error);
    });
}

void AdbManager::writePropertyDefinitions(const QString &deviceId,
                                          const QVector<QPair<QString, QString>> &values)
{
    const QString adbPath = m_adbPath;
    (void)QtConcurrent::run([this, deviceId, adbPath, values]() {
        PropertyWriteResult total;
        for (const QStringList &args : ConfigurationManagerOutput::setCommands(values))
            total.append(runPropertyWrite(adbPath, deviceId, args));
        emit propertyDefinitionsWritten(deviceId, false, total);
    });
}

void AdbManager::resetPropertyDefinitions(const QString &deviceId, const QStringList &names)
{
    const QString adbPath = m_adbPath;
    (void)QtConcurrent::run([this, deviceId, adbPath, names]() {
        PropertyWriteResult total;
        for (const QStringList &args : ConfigurationManagerOutput::resetCommands(names))
            total.append(runPropertyWrite(adbPath, deviceId, args));
        emit propertyDefinitionsWritten(deviceId, true, total);
    });
}

void AdbManager::runRawAdbCommand(const QString &command)
{
    const QString adbPath = m_adbPath;
    (void)QtConcurrent::run([this, adbPath, command]() {
        // Split the full command string into args, skipping leading "adb" token if present
        QStringList parts = QProcess::splitCommand(command);
        if (!parts.isEmpty() && parts.first().compare("adb", Qt::CaseInsensitive) == 0)
            parts.removeFirst();

        const AdbProcessResult result = AdbExecutor::run(adbPath, parts, 30000);
        if (result.timedOut) {
            emit errorOccurred(QStringLiteral("adb command timed out: ") + command);
            return;
        }
        const QString out = result.standardOutput;
        const QString err = result.standardError;
        emit rawAdbCommandFinished(out.isEmpty() && !err.isEmpty() ? err : out);
    });
}

void AdbManager::fetchDumpsys(const QString &deviceId, const QString &args)
{
    if (m_dumpsysInFlight) {
        m_pendingDumpsysDeviceId = deviceId;
        m_pendingDumpsysArgs = args;
        return;
    }

    m_dumpsysInFlight = true;
    const QString adbPath = m_adbPath;
    (void)QtConcurrent::run([this, deviceId, adbPath, args]() {
        QStringList pArgs = {QStringLiteral("-s"), deviceId,
                             QStringLiteral("shell"), QStringLiteral("dumpsys")};
        for (const QString &part : QProcess::splitCommand(args))
            pArgs << part;

        const AdbProcessResult result = AdbExecutor::run(adbPath, pArgs, 30000);
        QMetaObject::invokeMethod(this, [this, result, args]() {
            if (result.timedOut) {
                emit errorOccurred(QStringLiteral("dumpsys timed out: ") + args);
            } else if (!result.started) {
                emit errorOccurred(QStringLiteral("dumpsys failed: ") + result.errorMessage());
            } else {
                emit dumpsysFetched(result.standardOutput);
            }

            m_dumpsysInFlight = false;
            runPendingDumpsysRequest();
        }, Qt::QueuedConnection);
    });
}

void AdbManager::fetchDumpsysList(const QString &deviceId)
{
    const QString adbPath = m_adbPath;
    (void)QtConcurrent::run([this, deviceId, adbPath]() {
        const AdbProcessResult result =
            AdbExecutor::run(adbPath, AdbCommand::listDumpsysServices(deviceId), 15000);
        if (result.timedOut) {
            emit errorOccurred(QStringLiteral("dumpsys -l timed out"));
            return;
        }
        if (!result.started) {
            emit errorOccurred(QStringLiteral("dumpsys -l failed: ") + result.errorMessage());
            return;
        }
        emit dumpsysListFetched(DumpsysServiceListConverter::convert(result.standardOutput));
    });
}

void AdbManager::runCradleCommand(const QString &deviceId, const QStringList &args)
{
    const QString adbPath = m_adbPath;
    (void)QtConcurrent::run([this, deviceId, adbPath, args]() {
        const AdbProcessResult result =
            AdbExecutor::run(adbPath, AdbCommand::cradleCommand(deviceId, args), 30000);
        if (result.timedOut) {
            emit cradleCommandFinished(QString(), QStringLiteral("Command timed out"));
            return;
        }
        const QString out = result.standardOutput;
        const QString errOut = result.standardError.trimmed();
        emit cradleCommandFinished(out, errOut);
    });
}

bool AdbManager::getSettingValue(const QString &deviceId, const QString &group, const QString &setting,
                                 QString &value, QString &error)
{
    const AdbProcessResult result =
        AdbExecutor::run(m_adbPath, AdbCommand::getSetting(deviceId, group, setting), 3000);
    value = result.standardOutput.trimmed();
    if (!result.succeeded()) {
        error = result.errorMessage(QStringLiteral("Command timeout"));
        return false;
    }
    return true;
}

bool AdbManager::getPropertyValue(const QString &deviceId, const QString &property,
                                  QString &value, QString &error)
{
    const AdbProcessResult result =
        AdbExecutor::run(m_adbPath, AdbCommand::getProperty(deviceId, property), 3000);
    value = result.standardOutput.trimmed();
    if (!result.succeeded()) {
        error = result.errorMessage(QStringLiteral("Command timeout"));
        return false;
    }
    return true;
}

void AdbManager::runPendingDumpsysRequest()
{
    if (m_pendingDumpsysDeviceId.isEmpty())
        return;

    const QString deviceId = m_pendingDumpsysDeviceId;
    const QString args = m_pendingDumpsysArgs;
    m_pendingDumpsysDeviceId.clear();
    m_pendingDumpsysArgs.clear();
    fetchDumpsys(deviceId, args);
}
