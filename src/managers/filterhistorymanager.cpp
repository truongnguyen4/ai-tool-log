#include "filterhistorymanager.h"
#include <QLineEdit>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QEvent>
#include <QSettings>
#include <QCompleter>
#include <QStringListModel>
#include <QAbstractItemView>
#include <QTimer>

FilterHistoryManager::FilterHistoryManager(QObject *parent)
    : QObject(parent)
{}

void FilterHistoryManager::track(QLineEdit *lineEdit)
{
    lineEdit->installEventFilter(this);
}

void FilterHistoryManager::track(QLineEdit *lineEdit, const QString &settingsKey)
{
    m_settingsKeys[lineEdit] = settingsKey;
    loadHistory(lineEdit);
    attachCompleter(lineEdit);
    lineEdit->installEventFilter(this);
}

void FilterHistoryManager::trackDebounced(QLineEdit *lineEdit, const QString &settingsKey)
{
    m_settingsKeys[lineEdit] = settingsKey;
    loadHistory(lineEdit);
    attachCompleter(lineEdit);
    lineEdit->installEventFilter(this);

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(1000);
    m_debounceTimers[lineEdit] = timer;

    connect(lineEdit, &QLineEdit::textChanged, timer, [timer](const QString &) {
        timer->start(); // restart on every keystroke
    });
    connect(timer, &QTimer::timeout, this, [this, lineEdit]() {
        saveToHistory(lineEdit);
    });
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

    updateCompleter(lineEdit);
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

void FilterHistoryManager::loadHistory(QLineEdit *lineEdit)
{
    const auto it = m_settingsKeys.constFind(lineEdit);
    if (it == m_settingsKeys.constEnd())
        return;

    QSettings settings;
    settings.beginGroup(QStringLiteral("FilterHistory"));
    const QStringList saved = settings.value(it.value()).toStringList();
    settings.endGroup();

    if (!saved.isEmpty()) {
        m_history[lineEdit]      = saved;
        m_historyIndex[lineEdit] = saved.size();
    }
}

void FilterHistoryManager::persistHistory(QLineEdit *lineEdit)
{
    const auto it = m_settingsKeys.constFind(lineEdit);
    if (it == m_settingsKeys.constEnd())
        return;

    QSettings settings;
    settings.beginGroup(QStringLiteral("FilterHistory"));
    settings.setValue(it.value(), m_history.value(lineEdit));
    settings.endGroup();
}

void FilterHistoryManager::persistAll()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("FilterHistory"));
    for (auto it = m_settingsKeys.constBegin(); it != m_settingsKeys.constEnd(); ++it)
        settings.setValue(it.value(), m_history.value(it.key()));
    settings.endGroup();
}

void FilterHistoryManager::clearKeys(const QStringList &keys)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("FilterHistory"));
    for (auto it = m_settingsKeys.begin(); it != m_settingsKeys.end(); ++it) {
        if (keys.contains(it.value())) {
            m_history[it.key()].clear();
            m_historyIndex[it.key()] = 0;
            updateCompleter(it.key());
            settings.remove(it.value());
        }
    }
    settings.endGroup();
}

void FilterHistoryManager::attachCompleter(QLineEdit *lineEdit)
{
    auto *model = new QStringListModel(this);
    auto *completer = new QCompleter(model, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->popup()->setStyleSheet(
        "QListView {"
        "    background-color: #2d2d30;"
        "    color: #cccccc;"
        "    border: 1px solid #3e3e42;"
        "    selection-background-color: #0e639c;"
        "    selection-color: #ffffff;"
        "}");
    lineEdit->setCompleter(completer);
    m_completers[lineEdit] = completer;
    updateCompleter(lineEdit);
}

void FilterHistoryManager::updateCompleter(QLineEdit *lineEdit)
{
    QCompleter *completer = m_completers.value(lineEdit);
    if (!completer)
        return;

    // Show most recent entries first
    QStringList entries = m_history.value(lineEdit);
    std::reverse(entries.begin(), entries.end());
    qobject_cast<QStringListModel *>(completer->model())->setStringList(entries);
}

bool FilterHistoryManager::eventFilter(QObject *watched, QEvent *event){
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
        // Ignore focus-out that is just the completer popup appearing
        QCompleter *c = m_completers.value(lineEdit);
        if (c && c->popup()->isVisible())
            return QObject::eventFilter(watched, event);
        // Debounced fields save via their timer only — not on focus-out
        if (!m_debounceTimers.contains(lineEdit)) {
            if (!lineEdit->text().isEmpty())
                saveToHistory(lineEdit);
        }
    }

    if (event->type() == QEvent::FocusIn) {
        auto *focusEvent = static_cast<QFocusEvent *>(event);
        // Only auto-show history list when the user explicitly clicks
        // (not on tab navigation). Use a short delay so focus settles first.
        if (focusEvent->reason() == Qt::MouseFocusReason) {
            QTimer::singleShot(100, this, [this, lineEdit]() {
                QCompleter *completer = m_completers.value(lineEdit);
                if (completer && !m_history.value(lineEdit).isEmpty()) {
                    // Show all stored values regardless of current field content.
                    completer->setCompletionPrefix(QString());
                    completer->complete();
                }
            });
        }
    }

    return QObject::eventFilter(watched, event);
}
