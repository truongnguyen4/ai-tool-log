#include "filterhistorymanager.h"
#include <QLineEdit>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QEvent>

FilterHistoryManager::FilterHistoryManager(QObject *parent)
    : QObject(parent)
{}

void FilterHistoryManager::track(QLineEdit *lineEdit)
{
    lineEdit->installEventFilter(this);
}

void FilterHistoryManager::saveToHistory(QLineEdit *lineEdit)
{
    if (!lineEdit)
        return;

    const QString text = lineEdit->text().trimmed();
    if (text.isEmpty())
        return;

    QStringList &history = m_history[lineEdit];

    // Avoid duplicate at the end
    if (!history.isEmpty() && history.last() == text)
        return;

    // Move existing entry to end (dedup + promote)
    history.removeAll(text);
    history.append(text);

    if (history.size() > MAX_HISTORY_SIZE)
        history.removeFirst();

    m_historyIndex[lineEdit] = history.size();
    m_currentText[lineEdit].clear();
}

void FilterHistoryManager::navigateHistory(QLineEdit *lineEdit, bool goBack)
{
    if (!lineEdit)
        return;

    QStringList &history = m_history[lineEdit];
    if (history.isEmpty())
        return;

    // Initialise index on first navigation
    if (!m_historyIndex.contains(lineEdit)) {
        m_historyIndex[lineEdit] = history.size();
        m_currentText[lineEdit] = lineEdit->text();
    }

    int &index = m_historyIndex[lineEdit];

    // Snapshot the live text before we start navigating away from it
    if (index == history.size())
        m_currentText[lineEdit] = lineEdit->text();

    if (goBack) {
        if (index > 0)
            lineEdit->setText(history[--index]);
    } else {
        if (index < history.size() - 1) {
            lineEdit->setText(history[++index]);
        } else if (index == history.size() - 1) {
            ++index;
            lineEdit->setText(m_currentText[lineEdit]);
        }
    }
}

bool FilterHistoryManager::eventFilter(QObject *watched, QEvent *event)
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(watched);
    if (!lineEdit)
        return QObject::eventFilter(watched, event);

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Up) {
            navigateHistory(lineEdit, true);
            return true; // consumed
        }

        if (keyEvent->key() == Qt::Key_Down) {
            navigateHistory(lineEdit, false);
            return true; // consumed
        }

        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            saveToHistory(lineEdit);
            return false; // let returnPressed propagate
        }
    }

    if (event->type() == QEvent::FocusOut) {
        if (!lineEdit->text().isEmpty())
            saveToHistory(lineEdit);
    }

    return QObject::eventFilter(watched, event);
}
