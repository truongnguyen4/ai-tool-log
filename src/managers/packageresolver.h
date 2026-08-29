#ifndef PACKAGERESOLVER_H
#define PACKAGERESOLVER_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

class QTimer;

/**
 * Turns the pid on a log line into the process that wrote it.
 *
 * `logcat -v threadtime` prints pids, never package names, so a filter like
 * "pkg:com.example" has nothing to match until the pids are resolved. This
 * reads the device's process table (`ps`) and keeps a pid → process-name map,
 * refreshed while a capture runs and whenever a pid shows up that it does not
 * know yet.
 *
 * A pid the device has already reused points at the newer process; lines are
 * resolved as they arrive, which binds each one to the process that was alive
 * at that moment.
 */
class PackageResolver : public QObject
{
    Q_OBJECT

public:
    explicit PackageResolver(QObject *parent = nullptr);

    /** Follow @p serial; anything read from the previous device is dropped. */
    void setDevice(const QString &serial);
    /** Read the process table again, unless a read is already in flight. */
    void refresh();
    /** Keep the map fresh on a timer; on while a capture runs. */
    void setPolling(bool on);

    /** The process that logs as @p pid, or an empty string while unknown. */
    QString packageFor(const QString &pid) const { return m_processByPid.value(pid); }
    /** Running process names, sorted, without duplicates. */
    QStringList packages() const;
    bool isEmpty() const { return m_processByPid.isEmpty(); }

    /** Pid → process name, as `ps` printed it; empty when nothing parsed. */
    static QHash<QString, QString> parseProcessList(const QString &output);

signals:
    /** The map gained or changed entries: lines resolved before may now differ. */
    void mappingChanged();

private:
    QString                 m_serial;
    QHash<QString, QString> m_processByPid;
    QTimer                 *m_pollTimer = nullptr;
    bool                    m_inFlight  = false;
};

#endif // PACKAGERESOLVER_H
