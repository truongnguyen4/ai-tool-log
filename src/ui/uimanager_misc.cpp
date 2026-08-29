// UiManager: persisted layout, splitter defaults, shortcuts and dialog launchers.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "configurationcontroller.h"
#include "logsplitcontroller.h"
#include "settingsdialog.h"
#include "shortcutsdialog.h"
#include "tableconfig.h"

#include <QApplication>
#include <QHeaderView>
#include <QKeySequence>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QTableView>

namespace {
constexpr auto kLayoutGroup  = "Layout";
constexpr auto kHeaderSuffix = "/header";
} // namespace

QList<QSplitter *> UiManager::persistedSplitters() const
{
    return { m_ui->splitter, m_ui->splitterLogTables,
             m_ui->splitterMain, m_ui->splitterConfig,
             m_ui->splitterConfigTables, m_ui->splitterDumpsysOutput };
}

QList<QTableView *> UiManager::persistedTables() const
{
    return { m_ui->tableLog, m_ui->tableMarkLog,
             m_ui->tableSettings, m_ui->tableProperties,
             m_ui->tablePropertyDefinitions };
}

void UiManager::saveLayoutPreferences()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kLayoutGroup));

    for (QSplitter *splitter : persistedSplitters()) {
        if (splitter)
            settings.setValue(splitter->objectName(), splitter->saveState());
    }
    for (QTableView *table : persistedTables()) {
        if (table && table->horizontalHeader())
            settings.setValue(table->objectName() + QLatin1String(kHeaderSuffix),
                              table->horizontalHeader()->saveState());
    }

    settings.endGroup();
}

void UiManager::restoreLayoutPreferences()
{
    QSettings settings;
    settings.beginGroup(QLatin1String(kLayoutGroup));

    for (QSplitter *splitter : persistedSplitters()) {
        if (splitter && settings.contains(splitter->objectName()))
            splitter->restoreState(settings.value(splitter->objectName()).toByteArray());
    }
    for (QTableView *table : persistedTables()) {
        if (!table || !table->horizontalHeader())
            continue;
        const QString key = table->objectName() + QLatin1String(kHeaderSuffix);
        if (settings.contains(key))
            table->horizontalHeader()->restoreState(settings.value(key).toByteArray());
    }

    settings.endGroup();
}

void UiManager::setupSplittersAndMisc()
{
    m_ui->splitter->setSizes(QList<int>() << 250 << 1150);
    m_ui->splitterLogTables->setSizes(QList<int>() << 600 << 200);

    // Override defaults with persisted user preferences (if present).
    restoreLayoutPreferences();

    // F1 → Keyboard Shortcuts dialog (U5).
    auto *helpShortcut = new QShortcut(QKeySequence(Qt::Key_F1), m_mainWindow);
    connect(helpShortcut, &QShortcut::activated, this, [this]() {
        ShortcutsDialog dlg(m_mainWindow);
        dlg.exec();
    });
}

void UiManager::onAutoScrollToggled(bool checked)
{
    if (!checked)
        return;
    // Jump to the newest line in every visible pane, not just pane A.
    if (QTableView *table = m_ui->tableLog)
        table->scrollToBottom();
    if (m_logSplitController) {
        if (auto *paneB = m_logSplitController->paneB()) {
            if (paneB->table)
                paneB->table->scrollToBottom();
        }
    }
}

void UiManager::onAppSettingsClicked()
{
    QVector<bool> colVis;
    for (int c = 0; c < TableConfig::LogColumns::TOTAL_COLUMNS; ++c)
        colVis.append(!m_ui->tableLog->isColumnHidden(c));

    QVector<bool> propDefColVis;
    for (int c = 0; c < TableConfig::PropertyDefColumns::TOTAL_COLUMNS; ++c)
        propDefColVis.append(!m_ui->tablePropertyDefinitions->isColumnHidden(c));

    SettingsDialog dlg(QApplication::font(), colVis, propDefColVis, m_mainWindow);
    if (dlg.exec() == QDialog::Accepted) {
        applyAppFont(dlg.selectedFont());
        applyColumnVisibility(dlg.columnVisibility());
        m_configurationController->applyPropDefColumnVisibility(dlg.propDefColumnVisibility());

        const QStringList keys = dlg.keysToReset();
        if (!keys.isEmpty())
            m_historyManager->clearHistory(keys);
    }
}

