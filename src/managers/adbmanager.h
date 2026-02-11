#ifndef ADBMANAGER_H
#define ADBMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTimer>
#include <QMap>
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinition.h"

struct AdbDevice {
    QString id;
    QString name;
    bool isOnline;
};

class AdbManager : public QObject
{
    Q_OBJECT

public:
    static AdbManager& instance();
    
    // Delete copy constructor and assignment operator
    AdbManager(const AdbManager&) = delete;
    AdbManager& operator=(const AdbManager&) = delete;
    
    // Public methods
    QList<AdbDevice> getConnectedDevices();
    bool startLogcat(const QString &deviceId);
    void stopLogcat();
    bool isLogcatRunning() const;
    QString getAdbPath() const;
    void setAdbPath(const QString &path);
    
    // Current device management
    QString getCurrentDeviceId() const;
    void setCurrentDeviceId(const QString &deviceId);
    
    // Configuration methods
    void fetchSettings(const QString &deviceId);
    void fetchProperties(const QString &deviceId);
    bool setSetting(const QString &deviceId, const QString &group, const QString &setting, const QString &value, QString &error);
    bool setProperty(const QString &deviceId, const QString &property, const QString &value, QString &error);
    QString verifySetting(const QString &deviceId, const QString &group, const QString &setting);
    QString verifyProperty(const QString &deviceId, const QString &property);
    
    // Property Definition methods (SDK)
    void fetchPropertyDefinitions(const QString &deviceId);
    bool getPropertyDefinitionValue(const QString &deviceId, const QString &propertyId, QString &value, QString &error);
    bool setPropertyDefinitionValue(const QString &deviceId, const QString &propertyId, const QString &value, QString &error);
    
signals:
    void devicesChanged(const QList<AdbDevice> &devices);
    void logcatLineReceived(const QString &line);
    void logcatStarted();
    void logcatStopped();
    void errorOccurred(const QString &error);
    void settingsFetched(const QVector<SettingEntry> &settings);
    void propertiesFetched(const QVector<PropertyEntry> &properties);
    void propertyDefinitionsFetched(const QVector<PropertyDefinition> &propertyDefinitions);
    
private:
    explicit AdbManager(QObject *parent = nullptr);
    ~AdbManager();
    
    void detectDevices();
    void parseDeviceList(const QString &output);
    QString getDeviceName(const QString &deviceId);
    
    QString m_adbPath;
    QProcess *m_logcatProcess;
    QTimer *m_deviceDetectionTimer;
    QList<AdbDevice> m_connectedDevices;
    QString m_currentDeviceId;
    bool m_logcatRunning;
};

#endif // ADBMANAGER_H
