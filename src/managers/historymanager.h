#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <QObject>
#include <QHash>
#include <QStringList>

class QTimer;

// Generic per-name history store for any input that benefits from
// remembering prior values (filter fields, search boxes, command inputs,
// ...). Persists to QSettings under the "FilterHistory" group.
//
// Two main entry points:
//
//   saveHistory(name, value)       Append `value` to history for `name`.
//                                  By default the actual write is debounced
//                                  by ~2 seconds per name, so live-typing
//                                  doesn't flood storage with partial entries.
//
//   loadHistory(name)              Return the persisted history list for
//                                  `name`, oldest-first / most-recent-last.
class HistoryManager : public QObject
{
    Q_OBJECT
public:
    explicit HistoryManager(QObject *parent = nullptr);

    // Persist `value` under `name`. With debounce=true (default) the write
    // is deferred ~2s after the last call for the same `name` (collapses
    // bursts from live-typing). debounce=false writes synchronously.
    void saveHistory(const QString &name, const QString &value, bool debounce = true);

    // Returns the persisted history for `name`. Empty list if nothing saved.
    QStringList loadHistory(const QString &name) const;

    // Clear named histories from in-memory cache and from QSettings.
    void clearHistory(const QStringList &names);

    // Force-flush every pending debounced write (call on app close).
    void flush();

    static const int MAX_HISTORY_SIZE  = 50;
    static const int DEBOUNCE_INTERVAL_MS = 2000;

signals:
    /**
     * The stored history for @p name changed.
     *
     * Completer models listen for this instead of reloading on every
     * keystroke, which rebuilt the completion index on each character typed.
     */
    void historyChanged(const QString &name);

private:
    void persistImmediate(const QString &name, const QString &value);
    QStringList readFromSettings(const QString &name) const;
    void writeToSettings(const QString &name, const QStringList &history) const;

    mutable QHash<QString, QStringList> m_cache;          // name -> history
    QHash<QString, QTimer *>            m_pendingTimers;  // name -> debounce timer
    QHash<QString, QString>             m_pendingValues;  // name -> last value
};

#endif // HISTORYMANAGER_H
