#ifndef FILTERHISTORYMANAGER_H
#define FILTERHISTORYMANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>

class QLineEdit;
class QCompleter;
class QStringListModel;
class QTimer;

/**
 * FilterHistoryManager - SRP extraction from MainWindow (Issue #1/#9).
 *
 * Manages per-QLineEdit filter history with Up/Down keyboard navigation.
 * Install it as an event filter via track() instead of MainWindow::eventFilter.
 *
 * When a settingsKey is supplied to track(), the history for that field is
 * persisted via QSettings so it survives application restarts.
 */
class FilterHistoryManager : public QObject
{
    Q_OBJECT

public:
    explicit FilterHistoryManager(QObject *parent = nullptr);

    /** Install this manager as the event filter for a QLineEdit. */
    void track(QLineEdit *lineEdit);

    /**
     * Install as event filter and persist history under settingsKey.
     * The history is loaded immediately from QSettings.
     */
    void track(QLineEdit *lineEdit, const QString &settingsKey);

    /**
     * Like track(lineEdit, settingsKey) but auto-saves 1 second after the
     * user stops typing (debounce). Useful for filter-as-you-type fields.
     * Enter key and FocusOut still save immediately.
     */
    void trackDebounced(QLineEdit *lineEdit, const QString &settingsKey);

    /** Persist the current text of a line-edit into its history. */
    void saveToHistory(QLineEdit *lineEdit);

    /** Navigate history: goBack=true → older entry, false → newer entry. */
    void navigateHistory(QLineEdit *lineEdit, bool goBack);

    /** Save all persisted fields to QSettings. Call on app close. */
    void persistAll();

    /** Clear in-memory history and QSettings entries for the given settings keys.
     * Also clears the completer model for affected fields.
     */
    void clearKeys(const QStringList &keys);

    /** Return the QCompleter attached to a tracked line-edit, or nullptr. */
    QCompleter *completerFor(QLineEdit *lineEdit) const { return m_completers.value(lineEdit, nullptr); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static const int MAX_HISTORY_SIZE = 50;

    void loadHistory(QLineEdit *lineEdit);
    void persistHistory(QLineEdit *lineEdit);
    void attachCompleter(QLineEdit *lineEdit);
    void updateCompleter(QLineEdit *lineEdit);

    QMap<QLineEdit *, QStringList>    m_history;
    QMap<QLineEdit *, int>            m_historyIndex;
    QMap<QLineEdit *, QString>        m_currentText;
    QMap<QLineEdit *, QString>        m_settingsKeys;     // only for persisted fields
    QMap<QLineEdit *, QCompleter *>   m_completers;       // only for persisted fields
    QMap<QLineEdit *, QTimer *>       m_debounceTimers;   // only for debounced fields
};

#endif // FILTERHISTORYMANAGER_H
