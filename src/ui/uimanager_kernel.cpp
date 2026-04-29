// UiManager: kernel-log (dmesg) capture handlers.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "logmodel.h"
#include "marklogmodel.h"

#include <QRegularExpression>

void UiManager::onKernelClicked()
{
    AdbManager &mgr = AdbManager::instance();

    if (mgr.isDmesgRunning()) {
        mgr.stopDmesg();
        return;
    }

    if (m_currentDeviceId.isEmpty()) {
        flashStatus(tr("No device selected"));
        return;
    }

    m_pendingLines.clear();
    activeAllLogs().clear();
    activeFilteredLogs().clear();
    activeAllLogsIndex().clear();
    activeFilteredLogsIndex().clear();
    activeNextLogId() = 0;
    activeLogModel()->clear();
    activeMarkedRows().clear();
    activeMarkLogModel()->clear();
    updateFilterCount();

    if (!mgr.startDmesg(m_currentDeviceId)) {
        m_ui->statusbar->showMessage(tr("Failed to start dmesg"), 5000);
        return;
    }

    setKernelRunningVisuals(true);
    m_ui->statusbar->showMessage(tr("Kernel log started (adb shell dmesg -w)"), 5000);
}

void UiManager::parseDmesgLine(const QString &line)
{
    static const QRegularExpression re(
        R"(^\[\s*([\d.]+)\]\s*(?:\[([^\]]*)\])?\s*(.*)$)");

    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch()) return;

    LogEntry entry;
    entry.time    = m.captured(1).trimmed();
    entry.tag     = m.captured(2).trimmed();
    entry.message = m.captured(3).trimmed();
    if (entry.tag.isEmpty()) entry.tag = QStringLiteral("KERNEL");
    entry.level = QStringLiteral("I");

    QVector<LogEntry>&  all      = activeAllLogs();
    QVector<LogEntry>&  filtered = activeFilteredLogs();
    QHash<quint64,int>& allIdx   = activeAllLogsIndex();
    QHash<quint64,int>& filtIdx  = activeFilteredLogsIndex();
    quint64&            nextId   = activeNextLogId();

    entry.id = ++nextId;
    allIdx[entry.id] = all.size();
    all.append(entry);

    if (passesFilter(entry)) {
        filtIdx[entry.id] = filtered.size();
        filtered.append(entry);
        activeLogModel()->addLog(entry);
        if (m_ui->btnAutoScroll->isChecked())
            activeTableLog()->scrollToBottom();
        updateFilterCount();
    }
}

