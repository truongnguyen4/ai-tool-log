#ifndef ADBMANAGER_H
#define ADBMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTimer>
#include <QMap>

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
    
signals:
    void devicesChanged(const QList<AdbDevice> &devices);
    void logcatLineReceived(const QString &line);
    void logcatStarted();
    void logcatStopped();
    void errorOccurred(const QString &error);
    
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
