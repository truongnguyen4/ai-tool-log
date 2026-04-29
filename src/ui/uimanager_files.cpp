// UiManager: logcat start/clear/line-ingest handlers.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "adbcommand.h"
#include "adbexecutor.h"
#include "filterengine.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include <QDateTime>
#include <QtConcurrent>


void UiManager::onLogcatLineReceived(const QString &line)
{
    m_pendingLines.append(line);
    if (!m_batchFlushTimer->isActive())
        m_batchFlushTimer->start();
}

void UiManager::onStartClicked()
{
    AdbManager &mgr = AdbManager::instance();

    if (mgr.isLogcatRunning()) {
        mgr.stopLogcat();
        return;
    }

    if (m_currentDeviceId.isEmpty()) {
        m_ui->statusbar->showMessage("No device selected", 3000);
        return;
    }

    // Clear the active pane only — other pane keeps its data.
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

    if (!mgr.startLogcat(m_currentDeviceId)) {
        m_ui->statusbar->showMessage("Failed to start logcat", 5000);
    }
}

void UiManager::onClearClicked()
{
    activeAllLogs().clear();
    activeFilteredLogs().clear();
    activeAllLogsIndex().clear();
    activeFilteredLogsIndex().clear();
    activeNextLogId() = 0;
    activeLogModel()->clear();
    activeMarkedRows().clear();
    activeMarkLogModel()->clear();
    updateFilterCount();
    updateStatusBar();

    if (!m_currentDeviceId.isEmpty()) {
        const QString adbPath = AdbManager::instance().getAdbPath();
        const bool isDmesg = AdbManager::instance().isDmesgRunning();
        const QStringList args = isDmesg
            ? AdbCommand::clearDmesg(m_currentDeviceId)
            : AdbCommand::clearLogcat(m_currentDeviceId);
        const AdbProcessResult result = AdbExecutor::run(adbPath, args, 3000);
        const bool cleared = result.completed();
        m_ui->statusbar->showMessage(
            cleared ? (isDmesg ? "Cleared local logs and device dmesg buffer"
                               : "Cleared local logs and device logcat buffer")
                    : "Cleared local logs (device buffer clear failed)",
            3000);
    } else {
        m_ui->statusbar->showMessage("Cleared local logs", 3000);
    }
}

void UiManager::flushPendingLines()
{
    if (m_pendingLines.isEmpty()) {
        m_batchFlushTimer->stop();
        return;
    }

    QVector<QString> lines;
    lines.swap(m_pendingLines);

    // Pre-convert once; ids are assigned per-pane below.
    QVector<LogEntry> converted;
    converted.reserve(lines.size());
    for (const QString &line : lines) {
        LogEntry e = m_logConverter->convert(line);
        if (e.isValid()) converted.append(e);
    }
    if (converted.isEmpty()) return;

    const FilterCriteria criteria = buildFilterCriteria();

    auto ingestInto = [&](QVector<LogEntry>& all,
                          QVector<LogEntry>& filtered,
                          QHash<quint64,int>& allIdx,
                          QHash<quint64,int>& filtIdx,
                          quint64& nextId,
                          LogModel* model,
                          QTableView* table)
    {
        QVector<LogEntry> toAdd;
        toAdd.reserve(converted.size());
        for (LogEntry entry : converted) {
            entry.id = ++nextId;
            allIdx[entry.id] = all.size();
            all.append(entry);
            if (m_logFilterController->logFilter().passesFilter(entry, criteria)) {
                filtIdx[entry.id] = filtered.size();
                filtered.append(entry);
                toAdd.append(entry);
            }
        }
        if (!toAdd.isEmpty()) {
            model->addLogs(toAdd);
            if (m_ui->btnAutoScroll->isChecked() && table)
                table->scrollToBottom();
        }
    };

    // Active pane
    ingestInto(activeAllLogs(), activeFilteredLogs(),
               activeAllLogsIndex(), activeFilteredLogsIndex(),
               activeNextLogId(), activeLogModel(), activeTableLog());

    // Mirror to inactive pane when sync is on and split is active
    if (m_syncPanes && m_paneOverride < 0
        && m_logSplitController && m_logSplitController->isSplit()) {
        const int otherIdx = m_logSplitController->activeIsB() ? 0 : 1;
        m_paneOverride = otherIdx;
        ingestInto(activeAllLogs(), activeFilteredLogs(),
                   activeAllLogsIndex(), activeFilteredLogsIndex(),
                   activeNextLogId(), activeLogModel(), activeTableLog());
        m_paneOverride = -1;
    }

    m_rowResizeTimer->start();
    updateFilterCount();
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Kernel (dmesg)
// ─────────────────────────────────────────────────────────────────────────────

