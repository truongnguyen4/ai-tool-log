#include "adbmanager.h"
#include "adbcommand.h"
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
    process.start(m_adbPath, AdbCommand::listDevices());
    
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
        
        // If current device is not in the new list, clear it
        if (!m_currentDeviceId.isEmpty()) {
            bool found = false;
            for (const AdbDevice &device : newDevices) {
                if (device.id == m_currentDeviceId) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                m_currentDeviceId = "";
            }
        }
        
        emit devicesChanged(m_connectedDevices);
    }
}

QString AdbManager::getDeviceName(const QString &deviceId)
{
    QProcess process;
    process.start(m_adbPath, AdbCommand::getDeviceModel(deviceId));
    
    if (process.waitForFinished(2000)) {
        QString name = process.readAllStandardOutput().trimmed();
        if (!name.isEmpty()) {
            return name;
        }
    }
    
    return deviceId;
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
    QVector<SettingEntry> settings;
    int lineNum = 1;
    
    // Fetch settings from global, system, and secure namespaces
    QStringList namespaces = {"global", "system", "secure"};
    
    for (const QString &ns : namespaces) {
        QProcess process;
        
        process.start(m_adbPath, AdbCommand::listSettings(deviceId, ns));
        if (!process.waitForFinished(5000)) {
            emit errorOccurred(QString("Failed to fetch %1 settings").arg(ns));
            continue;
        }
        
        QString output = process.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        
        for (const QString &line : lines) {
            QString trimmedLine = line.trimmed();
            if (trimmedLine.isEmpty()) continue;
            
            // Parse format: "setting_name=value"
            int equalPos = trimmedLine.indexOf('=');
            if (equalPos > 0) {
                QString settingName = trimmedLine.left(equalPos);
                QString value = trimmedLine.mid(equalPos + 1);
                
                // Truncate long values for display
                QString displayValue = value;
                if (displayValue.length() > 50) {
                    displayValue = displayValue.left(47) + "...";
                }
                
                SettingEntry entry;
                entry.line = QString::number(lineNum++);
                entry.group = ns.at(0).toUpper() + ns.mid(1); // Capitalize first letter
                entry.setting = settingName;
                entry.value = displayValue;
                settings.append(entry);
            }
        }
    }
    
    emit settingsFetched(settings);
}

void AdbManager::fetchProperties(const QString &deviceId)
{
    QVector<PropertyEntry> properties;
    
    QProcess process;
    
    process.start(m_adbPath, AdbCommand::listProperties(deviceId));
    if (!process.waitForFinished(5000)) {
        emit errorOccurred("Failed to fetch system properties");
        return;
    }
    
    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    int lineNum = 1;
    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;
        
        // Parse format: "[property_name]: [value]"
        QRegularExpression regex("\\[([^\\]]+)\\]:\\s*\\[([^\\]]*)\\]");
        QRegularExpressionMatch match = regex.match(trimmedLine);
        
        if (match.hasMatch()) {
            QString propertyName = match.captured(1);
            QString value = match.captured(2);
            
            // Truncate long values for display
            QString displayValue = value;
            if (displayValue.length() > 50) {
                displayValue = displayValue.left(47) + "...";
            }
            
            PropertyEntry entry;
            entry.line = QString::number(lineNum++);
            entry.property = propertyName;
            entry.value = displayValue;
            properties.append(entry);
        }
    }
    
    emit propertiesFetched(properties);
}

bool AdbManager::setSetting(const QString &deviceId, const QString &group, const QString &setting, const QString &value, QString &error)
{
    QProcess process;
    QString namespace_ = group.toLower();
    
    process.start(m_adbPath, AdbCommand::putSetting(deviceId, namespace_, setting, value));
    if (!process.waitForFinished(5000)) {
        error = "Timeout while setting value";
        return false;
    }
    
    // Check for errors in stderr
    QString errorOutput = process.readAllStandardError();
    if (!errorOutput.isEmpty()) {
        error = errorOutput.trimmed();
        return false;
    }
    
    // Check exit code
    if (process.exitCode() != 0) {
        error = QString("Command failed with exit code %1").arg(process.exitCode());
        return false;
    }
    
    return true;
}

bool AdbManager::setProperty(const QString &deviceId, const QString &property, const QString &value, QString &error)
{
    QProcess process;
    
    process.start(m_adbPath, AdbCommand::setProperty(deviceId, property, value));
    if (!process.waitForFinished(5000)) {
        error = "Timeout while setting property";
        return false;
    }
    
    // Check for errors in stderr
    QString errorOutput = process.readAllStandardError();
    if (!errorOutput.isEmpty()) {
        error = errorOutput.trimmed();
        return false;
    }
    
    // Check exit code
    if (process.exitCode() != 0) {
        error = QString("Command failed with exit code %1").arg(process.exitCode());
        return false;
    }
    
    return true;
}

QString AdbManager::verifySetting(const QString &deviceId, const QString &group, const QString &setting)
{
    QProcess process;
    QString namespace_ = group.toLower();
    
    process.start(m_adbPath, AdbCommand::getSetting(deviceId, namespace_, setting));
    if (!process.waitForFinished(3000)) {
        return QString(); // Return empty on timeout
    }
    
    QString value = process.readAllStandardOutput().trimmed();
    return value;
}

QString AdbManager::verifyProperty(const QString &deviceId, const QString &property)
{
    QProcess process;
    
    process.start(m_adbPath, AdbCommand::getProperty(deviceId, property));
    if (!process.waitForFinished(3000)) {
        return QString(); // Return empty on timeout
    }
    
    QString value = process.readAllStandardOutput().trimmed();
    return value;
}

void AdbManager::fetchPropertyDefinitions(const QString &deviceId)
{
    QProcess process;
    process.start(m_adbPath, AdbCommand::getPropertyDefinitions(deviceId));
    
    if (!process.waitForFinished(5000)) {
        emit errorOccurred("Failed to fetch property definitions: timeout");
        return;
    }
    
    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();
    
    if (!errorOutput.isEmpty()) {
        qDebug() << "Property definition fetch error:" << errorOutput;
    }
    
    // Parse the output - format: id: 142, name: PROP_NAME, optional: true, persistence: false, ...
    QVector<PropertyDefinition> propertyDefinitions;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        
        // Skip empty lines
        if (trimmed.isEmpty()) {
            continue;
        }
        
        PropertyDefinition prop;
        
        // Split by comma to get key:value pairs
        QStringList pairs = trimmed.split(',');
        
        for (const QString &pair : pairs) {
            QString pairTrimmed = pair.trimmed();
            
            // Split by first colon only
            int colonPos = pairTrimmed.indexOf(':');
            if (colonPos > 0) {
                QString key = pairTrimmed.left(colonPos).trimmed().toLower();
                QString value = pairTrimmed.mid(colonPos + 1).trimmed();
                
                // Parse each attribute
                if (key == PropertyDefinition::KEY_ID) {
                    prop.id = value;
                }
                else if (key == PropertyDefinition::KEY_NAME) {
                    prop.name = value;
                }
                else if (key == PropertyDefinition::KEY_OPTIONAL) {
                    prop.optional = (value.toLower() == "true");
                }
                else if (key == PropertyDefinition::KEY_PERSISTENCE) {
                    // Handle both boolean and string formats
                    if (value.toLower() == "true" || value.toLower() == "false") {
                        prop.persistence = value.toLower();
                    } else {
                        prop.persistence = value;
                    }
                }
                else if (key == PropertyDefinition::KEY_VOLATILE) {
                    prop.isVolatile = (value.toLower() == "true");
                }
                else if (key == PropertyDefinition::KEY_IS_SUPPORTED || key == PropertyDefinition::KEY_IS_SUPPORTED_ALT) {
                    prop.isSupported = (value.toLower() == "true");
                }
                else if (key == PropertyDefinition::KEY_VALUE) {
                    // Handle null values
                    if (value.toLower() != "null") {
                        prop.value = value;
                    }
                }
                else if (key == PropertyDefinition::KEY_NEED_REBOOT || key == PropertyDefinition::KEY_NEED_REBOOT_ALT) {
                    prop.needReboot = (value.toLower() == "true");
                }
                else if (key == PropertyDefinition::KEY_READ_ONLY || key == PropertyDefinition::KEY_READ_ONLY_ALT) {
                    prop.readOnly = (value.toLower() == "true");
                }
                else if (key == PropertyDefinition::KEY_IS_LOADED || key == PropertyDefinition::KEY_IS_LOADED_ALT) {
                    prop.isLoaded = (value.toLower() == "true");
                }
                // Ignore: default, type, and other unknown fields
            }
        }
        
        // Add property if it has at minimum a name
        if (prop.isValid()) {
            propertyDefinitions.append(prop);
        }
    }
    
    qDebug() << "Fetched" << propertyDefinitions.size() << "property definitions";
    
    emit propertyDefinitionsFetched(propertyDefinitions);
}

bool AdbManager::getPropertyDefinitionValue(const QString &deviceId, const QString &propertyId, QString &value, QString &error)
{
    QProcess process;
    process.start(m_adbPath, AdbCommand::getCradleProperty(deviceId, propertyId));
    
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
    process.start(m_adbPath, AdbCommand::setCradleProperty(deviceId, propertyId, value));
    
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
