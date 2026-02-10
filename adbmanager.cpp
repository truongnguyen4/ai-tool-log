#include "adbmanager.h"
#include <QDebug>
#include <QRegularExpression>

AdbManager::AdbManager(QObject *parent)
    : QObject(parent)
    , m_adbPath("adb")
    , m_logcatProcess(nullptr)
    , m_deviceDetectionTimer(new QTimer(this))
    , m_logcatRunning(false)
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
    if (m_deviceDetectionTimer) {
        m_deviceDetectionTimer->stop();
    }
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
    process.start(m_adbPath, QStringList() << "devices" << "-l");
    
    if (!process.waitForFinished(3000)) {
        emit errorOccurred("Failed to execute adb devices command");
        return;
    }
    
    QString output = process.readAllStandardOutput();
    parseDeviceList(output);
}

void AdbManager::parseDeviceList(const QString &output)
{
    QList<AdbDevice> newDevices;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        if (line.startsWith("List of devices") || line.trimmed().isEmpty()) {
            continue;
        }
        
        // Parse line format: "deviceId    device product:... model:... device:... transport_id:..."
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            AdbDevice device;
            device.id = parts[0];
            device.isOnline = (parts[1] == "device");
            
            // Extract model name from the line
            QRegularExpression modelRegex("model:([^\\s]+)");
            QRegularExpressionMatch match = modelRegex.match(line);
            if (match.hasMatch()) {
                device.name = match.captured(1).replace('_', ' ');
            } else {
                device.name = device.id; // Fallback to ID if no model found
            }
            
            // Add device ID in parentheses
            device.name = QString("%1 (%2)").arg(device.name).arg(device.id);
            
            if (device.isOnline) {
                newDevices.append(device);
            }
        }
    }
    
    // Check if device list changed
    bool changed = (newDevices.size() != m_connectedDevices.size());
    if (!changed) {
        for (int i = 0; i < newDevices.size(); ++i) {
            if (newDevices[i].id != m_connectedDevices[i].id) {
                changed = true;
                break;
            }
        }
    }
    
    if (changed) {
        m_connectedDevices = newDevices;
        emit devicesChanged(m_connectedDevices);
    }
}

QString AdbManager::getDeviceName(const QString &deviceId)
{
    QProcess process;
    process.start(m_adbPath, QStringList() << "-s" << deviceId << "shell" << "getprop" << "ro.product.model");
    
    if (process.waitForFinished(2000)) {
        QString name = process.readAllStandardOutput().trimmed();
        if (!name.isEmpty()) {
            return name;
        }
    }
    
    return deviceId;
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
    QStringList args;
    args << "-s" << deviceId << "logcat" << "-v" << "time";
    
    m_logcatProcess->start(m_adbPath, args);
    
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

QString AdbManager::getAdbPath() const
{
    return m_adbPath;
}

void AdbManager::setAdbPath(const QString &path)
{
    m_adbPath = path;
}
