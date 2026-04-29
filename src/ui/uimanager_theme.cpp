// UiManager: theme/font/column visibility/status updates.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "colorscheme.h"
#include "tableconfig.h"
#include "themesheets.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QRegularExpression>
#include <QTextStream>


void UiManager::applyColumnVisibility(const QVector<bool> &vis)
{
    using namespace TableConfig::LogColumns;
    auto *pb = m_logSplitController ? m_logSplitController->paneB() : nullptr;
    for (int c = 0; c < vis.size(); ++c) {
        m_ui->tableLog->setColumnHidden(c, !vis[c]);
        m_ui->tableMarkLog->setColumnHidden(c, !vis[c]);
        if (pb && pb->table)     pb->table->setColumnHidden(c, !vis[c]);
        if (pb && pb->markTable) pb->markTable->setColumnHidden(c, !vis[c]);
    }
    if (vis.size() > PACKAGE) m_ui->groupBox_2->setVisible(vis[PACKAGE]);
    if (vis.size() > PID)     m_ui->groupBox_3->setVisible(vis[PID]);
    if (vis.size() > TIME)    m_ui->groupBox_6->setVisible(vis[TIME]);
    if (vis.size() > TAG)     m_ui->groupBox->setVisible(vis[TAG]);
    if (vis.size() > MESSAGE) m_ui->groupBox_5->setVisible(vis[MESSAGE]);
}

void UiManager::applyCurrentTheme()
{
    const auto resolved = ColorScheme::instance().resolvedMode();
    const QString sheet = (resolved == ColorScheme::Mode::Light)
                              ? ThemeSheets::lightStylesheet()
                              : m_darkStylesheet;
    if (qApp)
        qApp->setStyleSheet(sheet);
}

void UiManager::applyAppFont(const QFont &font)
{
    // Single source of truth: every widget (including the dynamically-created
    // pane B tables) inherits its font from the application via this loop.
    // No table-specific setFont calls live elsewhere — keep it that way.
    QApplication::setFont(font);
    for (QWidget *w : QApplication::topLevelWidgets()) {
        w->setFont(font);
        w->setStyleSheet(w->styleSheet());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Status Bar & Performance
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::updateMemoryUsage()
{
    QFile file("/proc/self/status");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.startsWith("VmRSS:")) {
            const QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 2)
                memoryUsage = parts[1].toLongLong() / 1024;
            break;
        }
    }
}

void UiManager::updateStatusBar()
{
    updateMemoryUsage();
    m_ui->statusbar->showMessage(
        QString("UTF-8  Lines: %1    Mem: %2MB  \u25cf %3")
            .arg(filteredLogs.size())
            .arg(memoryUsage)
            .arg(isPaused ? "Paused" : "Running"));
}

