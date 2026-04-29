// ConfigurationController: settings/properties row save + recreate buttons + refresh.
#include "configurationcontroller.h"
#include "ui_mainwindow.h"
#include "tableconfig.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "adbmanager.h"

#include "components/components.h"

#include <QHash>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>

void ConfigurationController::onSaveSettingClicked(int row)
{
    const QString deviceId = m_deviceIdProvider();
    if (deviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    if (row < 0 || row >= m_settingsModel->rowCount()) return;

    using namespace TableConfig::SettingsColumns;
    const QString group   = m_settingsModel->data(m_settingsModel->index(row, GROUP)).toString();
    const QString setting = m_settingsModel->data(m_settingsModel->index(row, SETTING)).toString();
    const QString value   = m_settingsModel->data(m_settingsModel->index(row, VALUE)).toString();

    AdbManager::instance().saveSettingAsync(row, deviceId, group, setting, value);
}

void ConfigurationController::onSettingSaveResult(int /*row*/, bool success,
                                                  const QString &group, const QString &setting,
                                                  const QString &newValue, const QString &verifiedValue,
                                                  const QString &error)
{
    if (!success) {
        QMessageBox::warning(m_mainWindow, "Failed to Set Value",
                             QString("Failed to set %1.%2:\n%3").arg(group, setting, error));
        return;
    }
    if (verifiedValue != newValue) {
        QMessageBox::warning(m_mainWindow, "Value Not Set",
                             QString("Setting %1.%2 could not be set.\n"
                                     "Expected: %3\nActual: %4\n\n"
                                     "This setting may be read-only or require special permissions.")
                                 .arg(group, setting, newValue,
                                      verifiedValue.isEmpty() ? "(null)" : verifiedValue));
    } else {
        m_ui->statusbar->showMessage(
            QString("Successfully set %1.%2 = %3").arg(group, setting, newValue), 3000);
    }
}

void ConfigurationController::onSavePropertyClicked(int row)
{
    const QString deviceId = m_deviceIdProvider();
    if (deviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    if (row < 0 || row >= m_propertiesModel->rowCount()) return;

    using namespace TableConfig::PropertiesColumns;
    const QString property = m_propertiesModel->data(m_propertiesModel->index(row, PROPERTY)).toString();
    const QString value    = m_propertiesModel->data(m_propertiesModel->index(row, VALUE)).toString();

    AdbManager::instance().savePropertyAsync(row, deviceId, property, value);
}

void ConfigurationController::onPropertySaveResult(int /*row*/, bool success,
                                                   const QString &property,
                                                   const QString &newValue, const QString &verifiedValue,
                                                   const QString &error)
{
    if (!success) {
        QMessageBox::warning(m_mainWindow, "Failed to Set Property",
                             QString("Failed to set %1:\n%2").arg(property, error));
        return;
    }
    if (verifiedValue != newValue) {
        QMessageBox::warning(m_mainWindow, "Property Not Set",
                             QString("Property %1 could not be set.\n"
                                     "Expected: %2\nActual: %3\n\n"
                                     "This property may be read-only or require special permissions.")
                                 .arg(property, newValue,
                                      verifiedValue.isEmpty() ? "(null)" : verifiedValue));
    } else {
        m_ui->statusbar->showMessage(
            QString("Successfully set %1 = %2").arg(property, newValue), 3000);
    }
}

static QPushButton *makeRowActionButton(const QString &tooltip, QWidget *parent)
{
    // Centralized icon-button factory — see src/ui/components/README.md.
    auto *btn = UiComponents::Button::icon(QIcon(":/icons/download.svg"),
                                           tooltip, parent,
                                           UiComponents::ButtonSize::Small);
    btn->setMaximumSize(50, 25);
    return btn;
}

void ConfigurationController::recreateSettingsButtons()
{
    using namespace TableConfig::SettingsColumns;
    const int rowCount = m_settingsModel->rowCount();

    for (int i = 0; i < rowCount; ++i) {
        QWidget *old = m_ui->tableSettings->indexWidget(m_settingsModel->index(i, ACTION));
        if (old) { m_ui->tableSettings->setIndexWidget(m_settingsModel->index(i, ACTION), nullptr); old->deleteLater(); }
    }
    for (int i = 0; i < rowCount; ++i) {
        QPushButton *btn = makeRowActionButton("Save this setting to device", nullptr);
        if (m_monitoringSettings) btn->setEnabled(false);
        m_ui->tableSettings->setIndexWidget(m_settingsModel->index(i, ACTION), btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { onSaveSettingClicked(i); });
    }
}

void ConfigurationController::recreatePropertiesButtons()
{
    using namespace TableConfig::PropertiesColumns;
    const int rowCount = m_propertiesModel->rowCount();

    for (int i = 0; i < rowCount; ++i) {
        QWidget *old = m_ui->tableProperties->indexWidget(m_propertiesModel->index(i, ACTION));
        if (old) { m_ui->tableProperties->setIndexWidget(m_propertiesModel->index(i, ACTION), nullptr); old->deleteLater(); }
    }
    for (int i = 0; i < rowCount; ++i) {
        QPushButton *btn = makeRowActionButton("Save this property to device", nullptr);
        if (m_monitoringProperties) btn->setEnabled(false);
        m_ui->tableProperties->setIndexWidget(m_propertiesModel->index(i, ACTION), btn);
        connect(btn, &QPushButton::clicked, this, [this, i]() { onSavePropertyClicked(i); });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Settings & Properties Refresh / Fetched (moved from UiManager)
// ─────────────────────────────────────────────────────────────────────────────

void ConfigurationController::onRefreshSettingsClicked()
{
    const QString deviceId = m_deviceIdProvider();
    if (deviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    AdbManager::instance().fetchSettings(deviceId);
}

void ConfigurationController::onRefreshPropertiesClicked()
{
    const QString deviceId = m_deviceIdProvider();
    if (deviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return;
    }
    AdbManager::instance().fetchProperties(deviceId);
}

void ConfigurationController::onSettingsFetched(const QVector<SettingEntry> &settings)
{
    if (m_monitoringSettings) {
        // Monitor path: value-only update (no insert), no filter rebuild,
        // no per-row button recreation — keeps the user's filter and any
        // disabled-state intact while ticking every 500 ms.
        m_settingsModel->updateSettings(settings, /*allowInsert=*/false);
        m_settingsRefreshBusy = false;
        return;
    }
    // Non-monitor path: full replace so the table aligns exactly with the
    // focused device (entries that no longer exist are removed).
    m_settingsModel->setSettings(settings);
    m_settingsModel->reapplyFilter();
    recreateSettingsButtons();
}

void ConfigurationController::onPropertiesFetched(const QVector<PropertyEntry> &properties)
{
    if (m_monitoringProperties) {
        m_propertiesModel->updateProperties(properties, /*allowInsert=*/false);
        m_propertiesRefreshBusy = false;
        return;
    }
    // Non-monitor path: full replace so the table aligns exactly with the
    // focused device.
    m_propertiesModel->setProperties(properties);
    m_propertiesModel->reapplyFilter();
    recreatePropertiesButtons();
}
