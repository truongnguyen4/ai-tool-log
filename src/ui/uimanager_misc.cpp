// UiManager: layout/splitters/dialog launchers/shortcuts.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "configurationcontroller.h"
#include "settingsdialog.h"
#include "shortcutsdialog.h"
#include "tableconfig.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeySequence>
#include <QLabel>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>

void UiManager::saveLayoutPreferences()
{
    QSettings s;
    s.beginGroup(QStringLiteral("Layout"));

    // Splitter geometries (Qt provides QSplitter::saveState / restoreState).
    for (QSplitter *sp : { m_ui->splitter, m_ui->splitterLogTables,
                           m_ui->splitterMain, m_ui->splitterConfig,
                           m_ui->splitterConfigTables, m_ui->splitterDumpsysOutput }) {
        if (sp) s.setValue(sp->objectName(), sp->saveState());
    }

    // Header column widths (one entry per relevant table).
    for (QTableView *tv : { m_ui->tableLog, m_ui->tableMarkLog,
                            m_ui->tableSettings, m_ui->tableProperties,
                            m_ui->tablePropertyDefinitions }) {
        if (tv && tv->horizontalHeader())
            s.setValue(tv->objectName() + QStringLiteral("/header"),
                       tv->horizontalHeader()->saveState());
    }

    s.endGroup();
}

void UiManager::restoreLayoutPreferences()
{
    QSettings s;
    s.beginGroup(QStringLiteral("Layout"));

    for (QSplitter *sp : { m_ui->splitter, m_ui->splitterLogTables,
                           m_ui->splitterMain, m_ui->splitterConfig,
                           m_ui->splitterConfigTables, m_ui->splitterDumpsysOutput }) {
        if (sp && s.contains(sp->objectName()))
            sp->restoreState(s.value(sp->objectName()).toByteArray());
    }

    for (QTableView *tv : { m_ui->tableLog, m_ui->tableMarkLog,
                            m_ui->tableSettings, m_ui->tableProperties,
                            m_ui->tablePropertyDefinitions }) {
        const QString key = tv->objectName() + QStringLiteral("/header");
        if (tv && tv->horizontalHeader() && s.contains(key))
            tv->horizontalHeader()->restoreState(s.value(key).toByteArray());
    }

    s.endGroup();
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

void UiManager::onColumnsClicked()
{
    QDialog dialog(m_mainWindow);
    dialog.setWindowTitle("Column Visibility");
    dialog.setMinimumWidth(300);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *titleLabel = new QLabel("Select columns to display:", &dialog);
    titleLabel->setStyleSheet("font-weight: bold; margin-bottom: 10px;");
    layout->addWidget(titleLabel);

    const QStringList columnNames = {"Date", "Time", "PID", "TID", "Package", "Lvl", "Tag", "Message"};
    QVector<QCheckBox *> checkboxes;
    for (int i = 0; i < columnNames.size(); ++i) {
        QCheckBox *cb = new QCheckBox(columnNames[i], &dialog);
        cb->setChecked(!m_ui->tableLog->isColumnHidden(i));
        checkboxes.append(cb);
        layout->addWidget(cb);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Inherits qApp's themed stylesheet; no inline overrides.

    if (dialog.exec() == QDialog::Accepted) {
        auto *pb = m_logSplitController ? m_logSplitController->paneB() : nullptr;
        for (int i = 0; i < checkboxes.size(); ++i) {
            const bool hidden = !checkboxes[i]->isChecked();
            m_ui->tableLog->setColumnHidden(i, hidden);
            m_ui->tableMarkLog->setColumnHidden(i, hidden);
            if (pb && pb->table)     pb->table->setColumnHidden(i, hidden);
            if (pb && pb->markTable) pb->markTable->setColumnHidden(i, hidden);
        }
    }
}

void UiManager::onAutoScrollToggled(bool checked)
{
    if (checked) m_ui->tableLog->scrollToBottom();
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

