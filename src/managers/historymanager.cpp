#include "historymanager.h"

#include <QSettings>
#include <QTimer>

namespace {
constexpr auto kSettingsGroup = "FilterHistory";
}

HistoryManager::HistoryManager(QObject *parent)
    : QObject(parent)
{}

void HistoryManager::saveHistory(const QString &name, const QString &value, bool debounce)
{
    if (name.isEmpty())
        return;

    if (!debounce) {
        persistImmediate(name, value);
        return;
    }

    m_pendingValues[name] = value;

    QTimer *&timer = m_pendingTimers[name];
    if (!timer) {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(DEBOUNCE_INTERVAL_MS);
        connect(timer, &QTimer::timeout, this, [this, name]() {
            const QString pending = m_pendingValues.take(name);
            persistImmediate(name, pending);
        });
    }
    timer->start();
}

QStringList HistoryManager::loadHistory(const QString &name) const
{
    if (name.isEmpty())
        return {};

    auto it = m_cache.constFind(name);
    if (it != m_cache.constEnd())
        return it.value();

    const QStringList stored = readFromSettings(name);
    m_cache.insert(name, stored);
    return stored;
}

void HistoryManager::clearHistory(const QStringList &names)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    for (const QString &name : names) {
        m_cache.remove(name);
        if (QTimer *t = m_pendingTimers.take(name)) {
            t->stop();
            t->deleteLater();
        }
        m_pendingValues.remove(name);
        settings.remove(name);
    }
    settings.endGroup();

    for (const QString &name : names)
        emit historyChanged(name);
}

void HistoryManager::flush()
{
    for (auto it = m_pendingTimers.begin(); it != m_pendingTimers.end(); ++it) {
        QTimer *t = it.value();
        if (!t)
            continue;
        if (t->isActive()) {
            t->stop();
            const QString name = it.key();
            const QString value = m_pendingValues.take(name);
            persistImmediate(name, value);
        }
    }
}

void HistoryManager::persistImmediate(const QString &name, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return;

    QStringList history = loadHistory(name);

    // Already the most recent entry: nothing to write, and no reason to make
    // every listening completer rebuild its model.
    if (!history.isEmpty() && history.constLast() == trimmed)
        return;

    // Dedup + promote to most-recent.
    history.removeAll(trimmed);
    history.append(trimmed);
    while (history.size() > MAX_HISTORY_SIZE)
        history.removeFirst();

    m_cache.insert(name, history);
    writeToSettings(name, history);
    emit historyChanged(name);
}

QStringList HistoryManager::readFromSettings(const QString &name) const
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    const QStringList stored = settings.value(name).toStringList();
    settings.endGroup();
    return stored;
}

void HistoryManager::writeToSettings(const QString &name, const QStringList &history) const
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(name, history);
    settings.endGroup();
}
