#include "adbmanager.h"
#include "adbcommand.h"
#include "propertydefinitionconverter.h"
#include <QDebug>
#include <QRegularExpression>
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
    stopLogcat();
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
    QProcess process;
    process.start(m_adbPath, AdbCommand::listDevices());

    const bool finished = process.waitForFinished(3000);

    if (!finished) {
        const QString errMsg = QString("Failed to execute adb devices command (QProcess::ProcessError %1)")
                                   .arg(static_cast<int>(process.error()));
        qWarning() << "[AdbManager] detectDevices:" << errMsg;
        emit errorOccurred(errMsg);
        return;
    }

    const QString stdErr = process.readAllStandardError().trimmed();
    if (!stdErr.isEmpty())
        qWarning() << "[AdbManager] detectDevices: stderr =" << stdErr;

    QString output = process.readAllStandardOutput();
    parseDeviceList(output);
}

void AdbManager::parseDeviceList(const QString &output)
{
    QList<AdbDevice> newDevices;
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        if (line.startsWith("List of devices") || line.trimmed().isEmpty())
            continue;

        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if (parts.size() < 2) {
            continue;
        }

        AdbDevice device;
        device.id = parts[0];
        device.isOnline = (parts[1] == "device");

        QRegularExpressionMatch m = QRegularExpression("model:([^\\s]+)").match(line);
        device.name = m.hasMatch() ? m.captured(1).replace('_', ' ') : device.id;
        device.name = QString("%1 (%2)").arg(device.name, device.id);

        if (device.isOnline)
            newDevices.append(device);
    }

    bool changed = (newDevices.size() != m_connectedDevices.size());
    if (!changed) {
        for (int i = 0; i < newDevices.size(); ++i)
            if (newDevices[i].id != m_connectedDevices[i].id) { changed = true; break; }
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
    m_logcatProcess = new QProcess(this);
    
    // Connect signals
    connect(m_logcatProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        while (m_logcatProcess && m_logcatProcess->canReadLine()) {
            QString line = m_logcatProcess->readLine().trimmed();
            if (!line.isEmpty()) {
                emit logcatLineReceived(line);
            }
        }
    });
    
    connect(m_logcatProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        QString errorMsg = QString("Logcat process error: %1").arg(error);
        emit errorOccurred(errorMsg);
        m_logcatRunning = false;
    });
    
    connect(m_logcatProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitCode);
        Q_UNUSED(exitStatus);
        m_logcatRunning = false;
        emit logcatStopped();
    });
    
    // Start logcat with time format
    m_logcatProcess->start(m_adbPath, AdbCommand::startLogcat(deviceId));
    if (!m_logcatProcess->waitForStarted(3000)) {
        emit errorOccurred("Failed to start logcat");
        delete m_logcatProcess;
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
        m_logcatProcess->kill();
        m_logcatProcess->waitForFinished(1000);
        m_logcatProcess->deleteLater();
        m_logcatProcess = nullptr;
    }
    m_logcatRunning = false;
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

    m_dmesgProcess = new QProcess(this);
    // Merge stderr into stdout so we can inspect error messages
    m_dmesgProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_dmesgProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        while (m_dmesgProcess && m_dmesgProcess->canReadLine()) {
            QString line = m_dmesgProcess->readLine().trimmed();
            if (!line.isEmpty()) {
                emit dmesgLineReceived(line);
            }
        }
    });

    connect(m_dmesgProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        emit errorOccurred(QString("Dmesg process error: %1").arg(error));
        m_dmesgRunning = false;
    });

    connect(m_dmesgProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        Q_UNUSED(exitStatus);
        // Read any remaining output (error text)
        const QString remaining = QString::fromLocal8Bit(m_dmesgProcess
            ? m_dmesgProcess->readAll() : QByteArray()).trimmed();
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
        emit dmesgStopped();
    });

    m_dmesgProcess->start(m_adbPath, AdbCommand::startDmesg(deviceId));
    if (!m_dmesgProcess->waitForStarted(3000)) {
        emit errorOccurred("Failed to start dmesg");
        delete m_dmesgProcess;
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
        m_dmesgUserStopped = true;
        m_dmesgProcess->kill();
        m_dmesgProcess->waitForFinished(1000);
        m_dmesgProcess->deleteLater();
        m_dmesgProcess = nullptr;
    }
    m_dmesgRunning = false;
    m_dmesgUserStopped = false;
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
    QtConcurrent::run([this, deviceId, adbPath]() {
        QVector<SettingEntry> settings;
        int lineNum = 1;

        const QStringList namespaces = {"global", "system", "secure"};
        for (const QString &ns : namespaces) {
            QProcess process;
            process.start(adbPath, AdbCommand::listSettings(deviceId, ns));
            if (!process.waitForFinished(5000)) {
                emit errorOccurred(QString("Failed to fetch %1 settings").arg(ns));
                continue;
            }

            const QStringList lines =
                QString::fromUtf8(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);

            for (const QString &line : lines) {
                const QString trimmed = line.trimmed();
                if (trimmed.isEmpty()) continue;
                const int eq = trimmed.indexOf('=');
                if (eq <= 0) continue;

                SettingEntry entry;
                entry.line    = QString::number(lineNum++);
                entry.group   = ns.at(0).toUpper() + ns.mid(1);
                entry.setting = trimmed.left(eq);
                entry.value   = trimmed.mid(eq + 1); // Issue #8: full value, no truncation
                settings.append(entry);
            }
        }

        emit settingsFetched(settings);
    });
}

void AdbManager::fetchProperties(const QString &deviceId)
{
    const QString adbPath = m_adbPath;

    // Issue #7: run in worker thread so the UI is never blocked
    QtConcurrent::run([this, deviceId, adbPath]() {
        QProcess process;
        process.start(adbPath, AdbCommand::listProperties(deviceId));
        if (!process.waitForFinished(5000)) {
            emit errorOccurred("Failed to fetch system properties");
            return;
        }

        QVector<PropertyEntry> properties;
        int lineNum = 1;
        const QRegularExpression regex("\\[([^\\]]+)\\]:\\s*\\[([^\\]]*)\\]");
        const QStringList lines =
            QString::fromUtf8(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            const QRegularExpressionMatch m = regex.match(line.trimmed());
            if (!m.hasMatch()) continue;

            PropertyEntry entry;
            entry.line     = QString::number(lineNum++);
            entry.property = m.captured(1);
            entry.value    = m.captured(2); // Issue #8: full value, no truncation
            properties.append(entry);
        }

        emit propertiesFetched(properties);
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

    QtConcurrent::run([this, row, deviceId, group, setting, newValue, adbPath]() {
        const QString ns = group.toLower();

        QProcess setProc;
        setProc.start(adbPath, AdbCommand::putSetting(deviceId, ns, setting, newValue));
        if (!setProc.waitForFinished(5000)) {
            emit settingSaveResult(row, false, group, setting, newValue, {}, "Timeout while setting value");
            return;
        }
        const QString errOut = setProc.readAllStandardError().trimmed();
        if (!errOut.isEmpty() || setProc.exitCode() != 0) {
            emit settingSaveResult(row, false, group, setting, newValue, {},
                errOut.isEmpty()
                    ? QString("Command failed with exit code %1").arg(setProc.exitCode())
                    : errOut);
            return;
        }

        QProcess verifyProc;
        verifyProc.start(adbPath, AdbCommand::getSetting(deviceId, ns, setting));
        verifyProc.waitForFinished(3000);
        emit settingSaveResult(row, true, group, setting, newValue,
                               verifyProc.readAllStandardOutput().trimmed(), {});
    });
}

void AdbManager::savePropertyAsync(int row, const QString &deviceId,
                                    const QString &property, const QString &newValue)
{
    const QString adbPath = m_adbPath;

    QtConcurrent::run([this, row, deviceId, property, newValue, adbPath]() {
        QProcess setProc;
        setProc.start(adbPath, AdbCommand::setProperty(deviceId, property, newValue));
        if (!setProc.waitForFinished(5000)) {
            emit propertySaveResult(row, false, property, newValue, {}, "Timeout while setting property");
            return;
        }
        const QString errOut = setProc.readAllStandardError().trimmed();
        if (!errOut.isEmpty() || setProc.exitCode() != 0) {
            emit propertySaveResult(row, false, property, newValue, {},
                errOut.isEmpty()
                    ? QString("Command failed with exit code %1").arg(setProc.exitCode())
                    : errOut);
            return;
        }

        QProcess verifyProc;
        verifyProc.start(adbPath, AdbCommand::getProperty(deviceId, property));
        verifyProc.waitForFinished(3000);
        emit propertySaveResult(row, true, property, newValue,
                                verifyProc.readAllStandardOutput().trimmed(), {});
    });
}

void AdbManager::fetchPropertyDefinitions(const QString &deviceId)
{
    const QString adbPath = m_adbPath;

    QtConcurrent::run([this, deviceId, adbPath]() {
        QProcess process;
        process.start(adbPath, AdbCommand::getPropertyDefinitions(deviceId));
        if (!process.waitForFinished(5000)) {
            emit errorOccurred("Failed to fetch property definitions: timeout");
            return;
        }
        const QString errOut = process.readAllStandardError();
        if (!errOut.isEmpty()) {
            qDebug() << "Property definition fetch error:" << errOut;
            emit errorOccurred("Failed to fetch property definitions: " + errOut);
            return;
        }

        const QVector<PropertyDefinition> defs =
            PropertyDefinitionConverter::parseOutput(process.readAllStandardOutput());
        qDebug() << "Fetched" << defs.size() << "property definitions";
        emit propertyDefinitionsFetched(defs);
    });
}

void AdbManager::runRawAdbCommand(const QString &command)
{
    const QString adbPath = m_adbPath;
    QtConcurrent::run([this, adbPath, command]() {
        // Split the full command string into args, skipping leading "adb" token if present
        QStringList parts = command.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!parts.isEmpty() && parts.first().compare("adb", Qt::CaseInsensitive) == 0)
            parts.removeFirst();

        QProcess process;
        process.start(adbPath, parts);
        if (!process.waitForFinished(30000)) {
            emit errorOccurred(QStringLiteral("adb command timed out: ") + command);
            return;
        }
        const QString out = QString::fromUtf8(process.readAllStandardOutput());
        const QString err = QString::fromUtf8(process.readAllStandardError());
        emit rawAdbCommandFinished(out.isEmpty() && !err.isEmpty() ? err : out);
    });
}

void AdbManager::fetchDumpsys(const QString &deviceId, const QString &args)
{
    const QString adbPath = m_adbPath;
    QtConcurrent::run([this, deviceId, adbPath, args]() {
        QStringList pArgs = {QStringLiteral("-s"), deviceId,
                             QStringLiteral("shell"), QStringLiteral("dumpsys")};
        for (const QString &part : args.split(QLatin1Char(' '), Qt::SkipEmptyParts))
            pArgs << part;

        QProcess process;
        process.start(adbPath, pArgs);
        if (!process.waitForFinished(30000)) {
            emit errorOccurred(QStringLiteral("dumpsys timed out: ") + args);
            return;
        }
        emit dumpsysFetched(QString::fromUtf8(process.readAllStandardOutput()));
    });
}

void AdbManager::fetchDumpsysList(const QString &deviceId)
{
    const QString adbPath = m_adbPath;
    QtConcurrent::run([this, deviceId, adbPath]() {
        QProcess process;
        process.start(adbPath, AdbCommand::listDumpsysServices(deviceId));
        if (!process.waitForFinished(15000)) {
            emit errorOccurred(QStringLiteral("dumpsys -l timed out"));
            return;
        }
        const QString raw = QString::fromUtf8(process.readAllStandardOutput());
        QStringList services;
        for (const QString &line : raw.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QString svc = line.trimmed();
            if (!svc.isEmpty() && !svc.startsWith(QLatin1String("Currently")))
                services << svc;
        }
        services.sort(Qt::CaseInsensitive);
        emit dumpsysListFetched(services);
    });
}

void AdbManager::runCradleCommand(const QString &deviceId, const QStringList &args)
{
    const QString adbPath = m_adbPath;
    QtConcurrent::run([this, deviceId, adbPath, args]() {
        QProcess process;
        process.start(adbPath, AdbCommand::cradleCommand(deviceId, args));
        if (!process.waitForFinished(30000)) {
            emit cradleCommandFinished(QString(), QStringLiteral("Command timed out"));
            return;
        }
        const QString out   = QString::fromUtf8(process.readAllStandardOutput());
        const QString errOut = QString::fromUtf8(process.readAllStandardError()).trimmed();
        emit cradleCommandFinished(out, errOut);
    });
}

void AdbManager::setupReversePort(const QString &deviceId, quint16 devicePort, quint16 hostPort)
{
    const QString adbPath = m_adbPath;
    QtConcurrent::run([adbPath, deviceId, devicePort, hostPort]() {
        QProcess process;
        process.start(adbPath, AdbCommand::reversePort(deviceId, devicePort, hostPort));
        if (!process.waitForFinished(5000)) {
            qWarning() << "AdbManager::setupReversePort: timed out";
            return;
        }
        const QByteArray err = process.readAllStandardError().trimmed();
        if (!err.isEmpty())
            qWarning() << "AdbManager::setupReversePort:" << err;
        else
            qDebug() << "AdbManager::setupReversePort: device port" << devicePort
                     << "-> host port" << hostPort;
    });
}

void AdbManager::removeReversePort(const QString &deviceId, quint16 devicePort)
{
    const QString adbPath = m_adbPath;
    QProcess process;
    process.start(adbPath, QStringList() << "-s" << deviceId
                                        << "reverse" << "--remove"
                                        << QString("tcp:%1").arg(devicePort));
    if (!process.waitForFinished(3000))
        qWarning() << "AdbManager::removeReversePort: timed out";
    else
        qDebug() << "AdbManager::removeReversePort: removed device port" << devicePort;
}

bool AdbManager::getPropertyDefinitionValue(const QString &deviceId, const QString &propertyName, QString &value, QString &error)
{
    QProcess process;
    process.start(m_adbPath, AdbCommand::getPropertyDefinition(deviceId, propertyName));
    
    if (!process.waitForFinished(3000)) {
        error = "Command timeout";
        return false;
    }
    
    value = process.readAllStandardOutput().trimmed();
    QString errorOutput = process.readAllStandardError().trimmed();
    
    if (!errorOutput.isEmpty()) {
        error = errorOutput;
        return false;
    }
    
    return true;
}

bool AdbManager::setPropertyDefinitionValue(const QString &deviceId, const QString &propertyId, const QString &value, QString &error)
{
    QProcess process;
    process.start(m_adbPath, AdbCommand::setPropertyDefinition(deviceId, propertyId, value));
    
    if (!process.waitForFinished(3000)) {
        error = "Command timeout";
        return false;
    }
    
    QString errorOutput = process.readAllStandardError().trimmed();
    
    if (!errorOutput.isEmpty()) {
        error = errorOutput;
        return false;
    }
    
    return true;
}
