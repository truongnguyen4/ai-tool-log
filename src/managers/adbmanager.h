#ifndef ADBMANAGER_H
#define ADBMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QTimer>
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "configurationmanageroutput.h"
#include <QElapsedTimer>
#include <QMap>
#include <QPair>
#include <QSet>

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

    // --- Streaming captures: logcat and the kernel log ---
    //
    // Both work the same way. A device that is not online yet is waited for,
    // and its log read the moment it answers — early enough after a reboot to
    // catch the boot from its first lines. With @p followReboots the capture
    // outlives the device going away: it waits again and continues, with the
    // new boot's log after a reboot, or with the lines it had not shown yet
    // when only the connection dropped.
    /** Which log a capture reads. */
    enum class CaptureKind : quint8 { Logcat, Kernel };

    bool startLogcat(const QString &serial, bool followReboots);
    void stopLogcat();
    /** True from startLogcat() until the capture ends, waiting included. */
    bool isLogcatRunning() const;
    /** True while the capture waits for its device. */
    bool isLogcatWaiting() const;
    QString logcatSerial() const;

    bool startDmesg(const QString &serial, bool followReboots);
    void stopDmesg();
    bool isDmesgRunning() const;
    bool isDmesgWaiting() const;
    QString dmesgSerial() const;

    /** Change whether the running captures, and later ones, follow reboots. */
    void setCaptureFollowReboots(bool follow);
    /** The time of the last line @p kind showed, in that log's own format. */
    QString captureStamp(CaptureKind kind) const;

    // --- Async configuration fetch (Issue #7: non-blocking) ---
    void fetchSettings(const QString &deviceId);
    void fetchProperties(const QString &deviceId);
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

    // --- SDK configuration_manager properties (all asynchronous) ---
    /** `list --json`; answers with propertyDefinitionsFetched(). */
    void fetchPropertyDefinitions(const QString &deviceId);
    /** `set` for every name / value pair; answers with propertyDefinitionsWritten(). */
    void writePropertyDefinitions(const QString &deviceId,
                                  const QVector<QPair<QString, QString>> &values);
    /** `reset` of every name; answers with propertyDefinitionsWritten(reset = true). */
    void resetPropertyDefinitions(const QString &deviceId, const QStringList &names);

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
    /** The capture waits for @p serial; @p reconnecting when it was streaming before. */
    void logcatWaitingForDevice(const QString &serial, bool reconnecting);
    /** Lines flow from @p serial; @p newBoot when it rebooted since the last stream. */
    void logcatStreaming(const QString &serial, bool reconnected, bool newBoot);

    // Streaming dmesg
    void dmesgLineReceived(const QString &line);
    void dmesgStarted();
    void dmesgStopped();
    void dmesgFailed(const QString &reason);
    /** The capture waits for @p serial; @p reconnecting when it was streaming before. */
    void dmesgWaitingForDevice(const QString &serial, bool reconnecting);
    /** Lines flow from @p serial; @p newBoot when it rebooted since the last stream. */
    void dmesgStreaming(const QString &serial, bool reconnected, bool newBoot);

    // Async fetch results
    void settingsFetched(const QVector<SettingEntry> &settings);
    void propertiesFetched(const QVector<PropertyEntry> &properties);
    /** @p error is empty on success; @p deviceId says which device answered. */
    void propertyDefinitionsFetched(const QString &deviceId,
                                    const QVector<PropertyDefinition> &definitions,
                                    const QString &error);
    void propertyDefinitionsWritten(const QString &deviceId, bool reset,
                                    const PropertyWriteResult &result);
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

    /** One running capture: what it reads, from where, and where it got to. */
    struct CaptureSession {
        CaptureKind kind = CaptureKind::Logcat;
        QString   serial;
        QProcess *waitProcess   = nullptr;   ///< adb wait-for-device, before each stream
        QProcess *streamProcess = nullptr;
        QTimer   *retryTimer    = nullptr;
        QString   bootId;
        QString   lastStamp;                 ///< comparable key of the last line's time
        QString   lastStampText;             ///< that time as the log spells it
        QSet<QString> lastStampLines;        ///< lines shown with that time
        QElapsedTimer streamClock;
        int  streamCount     = 0;
        int  linesThisStream = 0;
        bool running       = false;
        bool waiting       = false;
        bool followReboots = true;
        bool skipResumed   = false;          ///< drop what a resumed stream repeats
        bool userStopped   = false;          ///< a stop of our own is not a failure
    };

    // Capture steps, shared by both logs; see the CaptureKind overloads above.
    bool startCapture(CaptureSession &session, const QString &serial, bool followReboots);
    void stopCapture(CaptureSession &session);
    void endCaptureSession(CaptureSession &session);
    void waitForCaptureDevice(CaptureSession &session, bool reconnecting);
    void scheduleCaptureRetry(CaptureSession &session);
    void runCaptureWait(CaptureSession &session);
    void startCaptureStream(CaptureSession &session, const QString &bootId);
    void handleCaptureLine(CaptureSession &session, const QString &line);

    QString      m_adbPath;
    CaptureSession m_logcat { CaptureKind::Logcat };
    CaptureSession m_kernel { CaptureKind::Kernel };
    QTimer      *m_deviceDetectionTimer;
    QList<AdbDevice> m_connectedDevices;
    QString      m_currentDeviceId;
    bool         m_detectInFlight    = false;
    bool         m_dumpsysInFlight   = false;
    QString      m_pendingDumpsysDeviceId;
    QString      m_pendingDumpsysArgs;
};

#endif // ADBMANAGER_H
