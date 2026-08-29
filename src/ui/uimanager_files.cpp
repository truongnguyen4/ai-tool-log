// UiManager: capture start/stop, buffer clearing and batched line ingestion.
//
// Logcat and kernel (dmesg) lines both arrive here as raw strings and are
// queued in m_pendingLines. The flush timer converts and inserts them in one
// batch, so a chatty device costs one model insert per flush rather than one
// per line.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "adbcommand.h"
#include "adbexecutor.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "threadtimelogconverter.h"
#include "dmesglogconverter.h"

#include <QScrollBar>
#include <QStatusBar>
#include <QTableView>

namespace {
/** adb buffer-clear timeout, in milliseconds. */
constexpr int kClearBufferTimeoutMs = 3000;
} // namespace

void UiManager::onLogLineReceived(const QString &line)
{
    m_pendingLines.append(line);
    if (m_batchFlushTimer && !m_batchFlushTimer->isActive())
        m_batchFlushTimer->start();
}

void UiManager::resetActivePaneLogs()
{
    activeAllLogs().clear();
    activeFilteredLogs().clear();
    activeAllLogsIndex().clear();
    activeFilteredLogsIndex().clear();
    activeNextLogId() = 0;
    activeMarkedRows().clear();
    activeLogModel()->clear();
    activeMarkLogModel()->clear();
    m_highlightRow     = -1;
    m_pendingCenterRow = -1;
    updateFilterCount();
}

bool UiManager::beginCapture(const LogConverterPtr &converter)
{
    if (m_currentDeviceId.isEmpty()) {
        flashStatus(tr("No device selected"));
        return false;
    }
    // Clear the active pane only — the other pane keeps its data.
    m_pendingLines.clear();
    resetActivePaneLogs();
    m_logConverter = converter;
    return true;
}

void UiManager::onStartClicked()
{
    AdbManager &adb = AdbManager::instance();
    if (adb.isLogcatRunning()) {
        adb.stopLogcat();
        return;
    }

    if (!beginCapture(LogConverterPtr(new ThreadtimeLogConverter())))
        return;

    if (!adb.startLogcat(m_currentDeviceId))
        flashStatus(tr("Failed to start logcat"));
}

void UiManager::onKernelClicked()
{
    AdbManager &adb = AdbManager::instance();
    if (adb.isDmesgRunning()) {
        adb.stopDmesg();
        return;
    }

    if (!beginCapture(LogConverterPtr(new DmesgLogConverter())))
        return;

    if (!adb.startDmesg(m_currentDeviceId)) {
        flashStatus(tr("Failed to start dmesg"));
        return;
    }

    setKernelRunningVisuals(true);
    flashStatus(tr("Kernel log started (adb shell dmesg -w)"));
}

void UiManager::onClearClicked()
{
    resetActivePaneLogs();
    updateStatusBar();

    if (m_currentDeviceId.isEmpty()) {
        flashStatus(tr("Cleared local logs"));
        return;
    }

    AdbManager &adb = AdbManager::instance();
    const bool kernel = adb.isDmesgRunning();
    const QStringList args = kernel ? AdbCommand::clearDmesg(m_currentDeviceId)
                                    : AdbCommand::clearLogcat(m_currentDeviceId);
    const AdbProcessResult result =
        AdbExecutor::run(adb.getAdbPath(), args, kClearBufferTimeoutMs);

    if (!result.completed())
        flashStatus(tr("Cleared local logs (device buffer clear failed)"));
    else if (kernel)
        flashStatus(tr("Cleared local logs and device dmesg buffer"));
    else
        flashStatus(tr("Cleared local logs and device logcat buffer"));
}

void UiManager::flushPendingLines()
{
    if (m_pendingLines.isEmpty()) {
        m_batchFlushTimer->stop();
        return;
    }

    QVector<QString> lines;
    lines.swap(m_pendingLines);

    // Convert once; log IDs are assigned per-pane during ingestion below.
    QVector<LogEntry> converted;
    converted.reserve(lines.size());
    for (const QString &line : lines) {
        LogEntry entry = m_logConverter->convert(line);
        if (entry.isValid())
            converted.append(std::move(entry));
    }
    if (converted.isEmpty())
        return;

    // The cached criteria; applyFilters() refreshes it whenever a filter
    // widget changes, so ingestion never has to parse the filter expressions.
    const FilterCriteria &criteria = m_logFilterController->criteria();
    const bool autoScroll = m_ui->btnAutoScroll->isChecked();

    auto ingestIntoActivePane = [this, &converted, &criteria, autoScroll]() {
        QVector<LogEntry>  &all      = activeAllLogs();
        QVector<LogEntry>  &filtered = activeFilteredLogs();
        QHash<quint64,int> &allIdx   = activeAllLogsIndex();
        QHash<quint64,int> &filtIdx  = activeFilteredLogsIndex();
        quint64            &nextId   = activeNextLogId();

        QVector<LogEntry> visible;
        visible.reserve(converted.size());
        all.reserve(all.size() + converted.size());

        for (const LogEntry &source : converted) {
            LogEntry entry = source;
            entry.id = ++nextId;
            allIdx.insert(entry.id, all.size());
            all.append(entry);

            if (!m_logFilterController->logFilter().passesFilter(entry, criteria))
                continue;
            filtIdx.insert(entry.id, filtered.size());
            filtered.append(entry);
            visible.append(std::move(entry));
        }

        if (visible.isEmpty())
            return;
        activeLogModel()->addLogs(visible);
        if (autoScroll) {
            if (QTableView *table = activeTableLog())
                table->scrollToBottom();
        }
    };

    ingestIntoActivePane();

    // Mirror into the inactive pane when sync is on and the split is active.
    if (m_syncPanes && m_paneOverride < 0
        && m_logSplitController && m_logSplitController->isSplit()) {
        m_paneOverride = m_logSplitController->activeIsB() ? 0 : 1;
        ingestIntoActivePane();
        m_paneOverride = -1;
    }

    m_rowResizeTimer->start();
    updateFilterCount();
}
