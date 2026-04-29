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
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include <QTimer>
#include <QtConcurrent>

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: File I/O
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onLoadFileClicked()
{
    const QString filePath = m_ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty()) {
        m_ui->statusbar->showMessage("Please enter a file path", 3000);
        return;
    }
    loadLogsFromFile(filePath);
}

void UiManager::onOpenFileClicked()
{
    const QString currentPath = m_ui->txtFilePath->text().trimmed();
    const QString defaultPath = currentPath.isEmpty() ? QDir::homePath() : currentPath;

    const QString filePath = QFileDialog::getOpenFileName(
        m_mainWindow, "Open Log File", defaultPath,
        "Log Files (*.log *.txt);;All Files (*.*)");

    if (!filePath.isEmpty()) {
        m_ui->txtFilePath->setText(filePath);
        loadLogsFromFile(filePath);
    }
}

void UiManager::onSaveFileClicked()
{
    QString filePath = m_ui->txtFilePath->text().trimmed();
    if (filePath.isEmpty()) {
        m_ui->statusbar->showMessage("Please enter a file path", 3000);
        return;
    }
    if (!filePath.endsWith(".log") && !filePath.endsWith(".txt"))
        filePath += ".log";

    QString errorMsg;
    if (m_fileManager.saveToFile(filePath, activeAllLogs(), errorMsg))
        m_ui->statusbar->showMessage(
            QString("Saved %1 log entries to %2").arg(activeAllLogs().size()).arg(filePath), 3000);
    else
        m_ui->statusbar->showMessage(QString("Failed to save: %1").arg(errorMsg), 5000);
}

void UiManager::loadLogsFromFile(const QString &filePath)
{
    if (m_isLoadingFile) {
        m_ui->statusbar->showMessage("File loading already in progress…", 3000);
        return;
    }
    m_isLoadingFile = true;
    m_ui->btnOpen->setEnabled(false);
    m_ui->statusbar->showMessage("Loading log file…", 0);

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

    activeAllLogs()         = std::move(res.entries);
    activeAllLogsIndex()    = std::move(res.allLogsIndex);
    activeNextLogId()       = res.nextLogId;
    activeMarkLogModel()->clear();
    if (res.converter) m_logConverter = res.converter;

    // Block signals while clearing filter fields (avoid spurious filter-change events)
    const QSignalBlocker b1(m_ui->txtFindMessage);
    const QSignalBlocker b2(m_ui->txtStartTime);
    const QSignalBlocker b3(m_ui->txtEndTime);
    const QSignalBlocker b4(m_ui->txtTagFilter);
    const QSignalBlocker b5(m_ui->txtPackageFilter);
    const QSignalBlocker b6(m_ui->txtPidFilter);
    m_ui->txtFindMessage->clear();
    m_ui->txtStartTime->clear();
    m_ui->txtEndTime->clear();
    m_ui->txtTagFilter->clear();
    m_ui->txtPackageFilter->clear();
    m_ui->txtPidFilter->clear();

    // All filters cleared → filteredLogs == allLogs (O(1) implicit-share copies)
    activeFilteredLogs()        = activeAllLogs();
    activeFilteredLogsIndex()   = activeAllLogsIndex();
    activeMarkedRows().clear();

    activeLogModel()->setLogs(activeFilteredLogs());
    activeLogModel()->setMarkedRows(&activeMarkedRows());

    updateFilterHighlighting();
    updateFilterCount();
    updateStatusBar();

    m_ui->statusbar->showMessage(
        QString("Loaded %1 log entries from %2 (Format: %3, Parsed: %4/%5)")
            .arg(entryCount)
            .arg(res.filePath)
            .arg(res.converter ? res.converter->name() : "Unknown")
            .arg(res.parsedCount)
            .arg(res.lineCount),
        5000);

    m_rowResizeTimer->start();
}
