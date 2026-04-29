#ifndef ADBMANAGER_H
#define ADBMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTimer>
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinition.h"
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

    AdbManager(const AdbManager&) = delete;
    AdbManager& operator=(const AdbManager&) = delete;

    // --- Device ---
    QList<AdbDevice> getConnectedDevices();
    QString getAdbPath() const;
    void setAdbPath(const QString &path);
    QString getCurrentDeviceId() const;
    void setCurrentDeviceId(const QString &deviceId);

    // --- Streaming logcat (line-by-line via signal) ---
    bool startLogcat(const QString &deviceId);
    void stopLogcat();
    bool isLogcatRunning() const;

    // --- Streaming dmesg / kernel log (line-by-line via signal) ---
    bool startDmesg(const QString &deviceId);
    void stopDmesg();
    bool isDmesgRunning() const;

    // --- Async configuration fetch (Issue #7: non-blocking) ---
    void fetchSettings(const QString &deviceId);
    void fetchProperties(const QString &deviceId);
    void fetchPropertyDefinitions(const QString &deviceId);
    // Run `adb shell dumpsys <args>` asynchronously (args = service + optional package)
    void fetchDumpsys(const QString &deviceId, const QString &args);
    // Run a raw adb command string asynchronously, e.g. "adb -s X shell cmd foo bar"
    void runRawAdbCommand(const QString &command);
    // Fetch list of available dumpsys services via `adb shell dumpsys -l`
    void fetchDumpsysList(const QString &deviceId);
    // Run `adb shell cmd cradle_manager <args>` asynchronously
    void runCradleCommand(const QString &deviceId, const QStringList &args);

    // --- Async save operations (Issue #7) ---
    void saveSettingAsync(int row, const QString &deviceId,
                          const QString &group, const QString &setting,
                          const QString &newValue);
    void savePropertyAsync(int row, const QString &deviceId,
                           const QString &property, const QString &newValue);

    // --- SDK property definitions ---
    bool getPropertyDefinitionValue(const QString &deviceId, const QString &propertyId,
                                    QString &value, QString &error);
    bool setPropertyDefinitionValue(const QString &deviceId, const QString &propertyId,
                                    const QString &value, QString &error);

    // --- Single-item synchronous fetchers (used by filtered monitoring) ---
    bool getSettingValue(const QString &deviceId, const QString &group, const QString &setting,
                         QString &value, QString &error);
    bool getPropertyValue(const QString &deviceId, const QString &property,
                          QString &value, QString &error);

signals:
    // Device
    void devicesChanged(const QList<AdbDevice> &devices);
    void errorOccurred(const QString &error);

    // Streaming logcat
    void logcatLineReceived(const QString &line);
    void logcatStarted();
    void logcatStopped();

    // Streaming dmesg
    void dmesgLineReceived(const QString &line);
    void dmesgStarted();
    void dmesgStopped();
    void dmesgFailed(const QString &reason);

    // Async fetch results
    void settingsFetched(const QVector<SettingEntry> &settings);
    void propertiesFetched(const QVector<PropertyEntry> &properties);
    void propertyDefinitionsFetched(const QVector<PropertyDefinition> &propertyDefinitions);
    void dumpsysFetched(const QString &output);
    void dumpsysListFetched(const QStringList &services);
    void rawAdbCommandFinished(const QString &output);
    void cradleCommandFinished(const QString &output, const QString &error);

    // Async save results
    void settingSaveResult(int row, bool success,
                           const QString &group, const QString &setting,
                           const QString &newValue, const QString &verifiedValue,
                           const QString &error);
    void propertySaveResult(int row, bool success,
                            const QString &property,
                            const QString &newValue, const QString &verifiedValue,
                            const QString &error);

private:
    explicit AdbManager(QObject *parent = nullptr);
    ~AdbManager();

    void detectDevices();
    void parseDeviceList(const QString &output);
    void runPendingDumpsysRequest();

    QString      m_adbPath;
    QProcess    *m_logcatProcess     = nullptr;
    QProcess    *m_dmesgProcess      = nullptr;
    QTimer      *m_deviceDetectionTimer;
    QList<AdbDevice> m_connectedDevices;
    QString      m_currentDeviceId;
    bool         m_logcatRunning     = false;
    bool         m_dmesgRunning      = false;
    bool         m_dmesgUserStopped  = false;
    bool         m_detectInFlight    = false;
    bool         m_dumpsysInFlight   = false;
    QString      m_pendingDumpsysDeviceId;
    QString      m_pendingDumpsysArgs;
};

#endif // ADBMANAGER_H
