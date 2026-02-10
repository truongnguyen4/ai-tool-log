#ifndef FILTERHISTORYMANAGER_H
#define FILTERHISTORYMANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>

class QLineEdit;

/**
 * FilterHistoryManager - SRP extraction from MainWindow (Issue #1/#9).
 *
 * Manages per-QLineEdit filter history with Up/Down keyboard navigation.
 * Install it as an event filter via track() instead of MainWindow::eventFilter.
 */
class FilterHistoryManager : public QObject
{
    Q_OBJECT

public:
    explicit FilterHistoryManager(QObject *parent = nullptr);

    /** Install this manager as the event filter for a QLineEdit. */
    void track(QLineEdit *lineEdit);

    /** Persist the current text of a line-edit into its history. */
    void saveToHistory(QLineEdit *lineEdit);

    /** Navigate history: goBack=true → older entry, false → newer entry. */
    void navigateHistory(QLineEdit *lineEdit, bool goBack);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static const int MAX_HISTORY_SIZE = 50;

    QMap<QLineEdit *, QStringList> m_history;
    QMap<QLineEdit *, int>         m_historyIndex;
    QMap<QLineEdit *, QString>     m_currentText;
};

#endif // FILTERHISTORYMANAGER_H
