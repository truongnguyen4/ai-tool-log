#include "adbmanager.h"
#include "adbcommand.h"
#include "adbexecutor.h"
#include "devicelistconverter.h"
#include "dumpsysservicelistconverter.h"
#include "propertieslistconverter.h"
#include "propertydefinitionconverter.h"
#include "settingslistconverter.h"
#include <QDebug>
#include <QMetaObject>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

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

bool AdbManager::startLogcat(const QString &deviceId)
{
    if (m_logcatRunning) {
        stopLogcat();
    }
    
    m_currentDeviceId = deviceId;
    auto *process = new QProcess(this);
    m_logcatProcess = process;
    
    // Connect signals
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        while (process->canReadLine()) {
            QString line = process->readLine().trimmed();
            if (!line.isEmpty()) {
                emit logcatLineReceived(line);
            }
        }
    });
    
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        QString errorMsg = QString("Logcat process error: %1").arg(error);
        emit errorOccurred(errorMsg);
        if (m_logcatProcess == process)
            m_logcatRunning = false;
    });
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitCode);
        Q_UNUSED(exitStatus);
        if (m_logcatProcess == process)
            m_logcatProcess = nullptr;
        const bool wasRunning = m_logcatRunning;
        m_logcatRunning = false;
        process->deleteLater();
        if (!wasRunning)
            return;
        emit logcatStopped();
    });
    
    // Start logcat with time format
    process->start(m_adbPath, AdbCommand::startLogcat(deviceId));
    if (!process->waitForStarted(3000)) {
        emit errorOccurred("Failed to start logcat");
        QObject::disconnect(process, nullptr, this, nullptr);
        delete process;
        m_logcatProcess = nullptr;
        return false;
    }

    m_logcatRunning = true;
    emit logcatStarted();
    return true;
}

void AdbManager::stopLogcat()
{
    if (m_logcatProcess) {
        QProcess *process = m_logcatProcess;
        m_logcatProcess = nullptr;
        QObject::disconnect(process, nullptr, this, nullptr);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(1000);
        }
        process->deleteLater();
    }
    const bool wasRunning = m_logcatRunning;
    m_logcatRunning = false;
    if (wasRunning)
        emit logcatStopped();
}

bool AdbManager::isLogcatRunning() const
{
    return m_logcatRunning;
}

bool AdbManager::startDmesg(const QString &deviceId)
{
    if (m_dmesgRunning) {
        stopDmesg();
    }

    auto *process = new QProcess(this);
    m_dmesgProcess = process;
    // Merge stderr into stdout so we can inspect error messages
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        while (process->canReadLine()) {
            QString line = process->readLine().trimmed();
            if (!line.isEmpty()) {
                emit dmesgLineReceived(line);
            }
        }
    });

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        emit errorOccurred(QString("Dmesg process error: %1").arg(error));
        if (m_dmesgProcess == process)
            m_dmesgRunning = false;
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus);
        // Read any remaining output (error text)
        const QString remaining = QString::fromLocal8Bit(process->readAll()).trimmed();
        if (m_dmesgProcess == process)
            m_dmesgProcess = nullptr;
        const bool wasRunning = m_dmesgRunning;
        m_dmesgRunning = false;
        // Only report failure when the process ended unexpectedly (not user-stopped)
        if (!m_dmesgUserStopped && exitCode != 0) {
            const QString lower = remaining.toLower();
            if (lower.contains("permission denied") || lower.contains("operation not permitted")
                    || lower.contains("not permitted") || lower.contains("eperm")) {
                emit dmesgFailed(tr("Root access required. Enable root mode on the device "
                                   "(Developer options \u2192 Root access) and try again."));
            } else {
                emit dmesgFailed(remaining.isEmpty()
                    ? tr("dmesg exited with code %1. Root access may be required.").arg(exitCode)
                    : remaining);
            }
        }
        m_dmesgUserStopped = false;
        process->deleteLater();
        if (wasRunning)
            emit dmesgStopped();
    });

    process->start(m_adbPath, AdbCommand::startDmesg(deviceId));
    if (!process->waitForStarted(3000)) {
        emit errorOccurred("Failed to start dmesg");
        QObject::disconnect(process, nullptr, this, nullptr);
        delete process;
        m_dmesgProcess = nullptr;
        return false;
    }

    m_dmesgRunning = true;
    emit dmesgStarted();
    return true;
}

void AdbManager::stopDmesg()
{
    if (m_dmesgProcess) {
        QProcess *process = m_dmesgProcess;
        m_dmesgUserStopped = true;
        m_dmesgProcess = nullptr;
        QObject::disconnect(process, nullptr, this, nullptr);
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(1000);
        }
        process->deleteLater();
    }
    const bool wasRunning = m_dmesgRunning;
    m_dmesgRunning = false;
    m_dmesgUserStopped = false;
    if (wasRunning)
        emit dmesgStopped();
}

bool AdbManager::isDmesgRunning() const
{
    return m_dmesgRunning;
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
            AdbExecutor::run(adbPath, AdbCommand::getPropertyDefinitions(deviceId), 5000);
        if (result.timedOut) {
            emit errorOccurred("Failed to fetch property definitions: timeout");
            // Emit an empty list so listeners clear stale data from a previous device.
            emit propertyDefinitionsFetched({});
            return;
        }
        if (!result.succeeded()) {
            qDebug() << "Property definition fetch error:" << result.errorMessage();
            emit errorOccurred("Failed to fetch property definitions: " + result.errorMessage());
            emit propertyDefinitionsFetched({});
            return;
        }

        const QVector<PropertyDefinition> defs =
            PropertyDefinitionConverter::convertOutput(result.standardOutput);
        qDebug() << "Fetched" << defs.size() << "property definitions";
        emit propertyDefinitionsFetched(defs);
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

bool AdbManager::getPropertyDefinitionValue(const QString &deviceId, const QString &propertyName, QString &value, QString &error)
{
    const AdbProcessResult result =
        AdbExecutor::run(m_adbPath, AdbCommand::getPropertyDefinition(deviceId, propertyName), 3000);

    value = result.standardOutput.trimmed();
    if (!result.succeeded()) {
        error = result.errorMessage(QStringLiteral("Command timeout"));
        return false;
    }
    
    return true;
}

bool AdbManager::setPropertyDefinitionValue(const QString &deviceId, const QString &propertyId, const QString &value, QString &error)
{
    const AdbProcessResult result =
        AdbExecutor::run(m_adbPath, AdbCommand::setPropertyDefinition(deviceId, propertyId, value), 3000);

    if (!result.succeeded()) {
        error = result.errorMessage(QStringLiteral("Command timeout"));
        return false;
    }
    
    return true;
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
