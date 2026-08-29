// UiManager: load/save log file workflows (async via QtConcurrent).
// uimanager_files.cpp.
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "logfiltercontroller.h"
#include "logentry.h"
#include "brieflogconverter.h"
#include "threadtimelogconverter.h"

#include <QDir>
#include <QFileDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QtConcurrent>

namespace {
/** How long the "loaded N entries" summary stays on the status bar. */
constexpr int kLoadMessageTimeoutMs = 5000;
constexpr auto kLogFileFilter = "Log Files (*.log *.txt);;All Files (*.*)";
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: File I/O
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onLoadFileClicked()
{
    const QString filePath = m_ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty()) {
        flashStatus(tr("Please enter a file path"));
        return;
    }
    loadLogsFromFile(filePath);
}

void UiManager::onOpenFileClicked()
{
    const QString currentPath = m_ui->txtFilePath->text().trimmed();
    const QString defaultPath = currentPath.isEmpty() ? QDir::homePath() : currentPath;

    const QString filePath = QFileDialog::getOpenFileName(
        m_mainWindow, tr("Open Log File"), defaultPath, tr(kLogFileFilter));
    if (filePath.isEmpty())
        return;

    m_ui->txtFilePath->setText(filePath);
    loadLogsFromFile(filePath);
}

void UiManager::onSaveFileClicked()
{
    QString filePath = m_ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty()) {
        flashStatus(tr("Please enter a file path"));
        return;
    }
    if (!filePath.endsWith(QLatin1String(".log")) && !filePath.endsWith(QLatin1String(".txt")))
        filePath += QLatin1String(".log");

    QString errorMsg;
    if (m_fileManager.saveToFile(filePath, activeAllLogs(), errorMsg))
        flashStatus(tr("Saved %1 log entries to %2")
                        .arg(activeAllLogs().size()).arg(filePath));
    else
        flashStatus(tr("Failed to save: %1").arg(errorMsg));
}

void UiManager::loadLogsFromFile(const QString &filePath)
{
    if (m_isLoadingFile) {
        flashStatus(tr("File loading already in progress…"));
        return;
    }
    m_isLoadingFile = true;
    m_ui->btnOpen->setEnabled(false);
    m_ui->statusbar->showMessage(tr("Loading log file…"), 0);

    QVector<LogConverterPtr> converters;
    converters.append(LogConverterPtr(new ThreadtimeLogConverter()));
    converters.append(LogConverterPtr(new BriefLogConverter()));

    QFuture<FileLoadResult> future = QtConcurrent::run(
        [filePath, converters]() -> FileLoadResult {
            FileLoadResult result;
            result.filePath = filePath;
            FileManager fm;
            QVector<LogEntry> raw = fm.readFromFileAuto(
                filePath, converters, result.converter, result.errorMsg);
            result.parsedCount = fm.getLastParsedCount();
            result.lineCount   = fm.getLastLineCount();
            if (raw.isEmpty()) return result;

            quint64 nextId = 0;
            result.entries.reserve(raw.size());
            result.allLogsIndex.reserve(raw.size());
            for (LogEntry &e : raw) {
                e.id = ++nextId;
                result.allLogsIndex[e.id] = result.entries.size();
                result.entries.append(std::move(e));
            }
            result.nextLogId = nextId;
            return result;
        });

    if (!m_fileLoaderWatcher) {
        m_fileLoaderWatcher = new QFutureWatcher<FileLoadResult>(this);
        connect(m_fileLoaderWatcher, &QFutureWatcher<FileLoadResult>::finished,
                this, &UiManager::onFileLoadFinished);
    } else {
        m_fileLoaderWatcher->cancel();
        m_fileLoaderWatcher->waitForFinished();
    }
    m_fileLoaderWatcher->setFuture(future);
}

void UiManager::onFileLoadFinished()
{
    m_isLoadingFile = false;
    m_ui->btnOpen->setEnabled(true);

    FileLoadResult res = m_fileLoaderWatcher->future().takeResult();

    if (res.entries.isEmpty() && !res.errorMsg.isEmpty()) {
        m_ui->statusbar->showMessage(
            QString("Failed to load file: %1").arg(res.errorMsg), 5000);
        return;
    }
    if (res.entries.isEmpty()) {
        m_ui->statusbar->showMessage("No valid log entries found in file", 5000);
        return;
    }

    const int entryCount = res.entries.size();

    activeAllLogs()      = std::move(res.entries);
    activeAllLogsIndex() = std::move(res.allLogsIndex);
    activeNextLogId()    = res.nextLogId;
    activeMarkedRows().clear();
    activeMarkLogModel()->clear();
    m_highlightRow = -1;
    if (res.converter)
        m_logConverter = res.converter;

    // Clear the column filters so the freshly loaded file is fully visible.
    // The keyword, highlight and level controls are deliberately left alone —
    // they are the user's current search, not a property of the old file.
    {
        const QList<QLineEdit *> columnFilters = {
            m_ui->txtFindMessage, m_ui->txtStartTime, m_ui->txtEndTime,
            m_ui->txtTagFilter,   m_ui->txtPackageFilter,
            m_ui->txtPidFilter,   m_ui->txtTidFilter,
        };
        for (QLineEdit *filter : columnFilters) {
            const QSignalBlocker blocker(filter);
            filter->clear();
        }
    }

    // Run the filter rather than assuming everything passes: a keyword or a
    // level floor may still be set, and the view must agree with the controls.
    applyFilters();

    m_ui->statusbar->showMessage(
        tr("Loaded %1 log entries from %2 (Format: %3, Parsed: %4/%5)")
            .arg(entryCount)
            .arg(res.filePath,
                 res.converter ? res.converter->name() : tr("Unknown"))
            .arg(res.parsedCount)
            .arg(res.lineCount),
        kLoadMessageTimeoutMs);

    m_rowResizeTimer->start();
}
