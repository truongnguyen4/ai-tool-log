#include "configurationcontroller.h"
#include "ui_mainwindow.h"
#include "propertydefinitionmodel.h"
#include "propertydefinitionjson.h"
#include "presetdialogs.h"
#include "jsonfileio.h"
#include "adbmanager.h"

#include <QDir>
#include <QFileDialog>
#include <QJsonDocument>
#include <QMessageBox>

namespace {
constexpr auto kJsonFilter = "JSON Files (*.json);;All Files (*)";
} // namespace

void ConfigurationController::onSavePropertySet()
{
    const auto &defs = m_propertyDefinitionModel->getPropertyDefinitions();
    if (defs.isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("Save Property Set"),
                                 tr("There are no property definitions to save."));
        return;
    }

    const QString name = PresetDialogs::askPresetName(
        m_mainWindow, tr("Save Property Set"),
        tr("Enter a name for this property set:"));
    if (name.isEmpty()) return;

    QString err;
    if (!m_propDefStore.savePreset(name, PropertyDefinitionJson::toBytes(defs), err)) {
        QMessageBox::critical(m_mainWindow, tr("Save Failed"), err);
        return;
    }
    m_ui->statusbar->showMessage(tr("Property set \"%1\" saved.").arg(name), 3000);
}

void ConfigurationController::onLoadPropertySet()
{
    const QString name = PresetDialogs::pickPresetWithDelete(
        m_mainWindow, tr("Load Property Set"),
        tr("Select a property set to load:"), m_propDefStore);
    if (name.isEmpty()) return;

    QString err;
    const auto defs = PropertyDefinitionJson::fromBytes(m_propDefStore.loadPreset(name), err);
    if (!err.isEmpty()) {
        QMessageBox::critical(m_mainWindow, tr("Load Failed"), err);
        return;
    }

    m_propertyDefinitionModel->setPropertyDefinitions(defs);
    updatePropertyNamesCompleter();
    if (!m_deviceIdProvider().isEmpty()) {
        m_ui->statusbar->showMessage(tr("Loading current property values from device..."), 0);
        AdbManager::instance().fetchPropertyDefinitions(m_deviceIdProvider());
    }
}

void ConfigurationController::onExportPropertySet()
{
    const auto &defs = m_propertyDefinitionModel->getPropertyDefinitions();
    if (defs.isEmpty()) {
        QMessageBox::information(m_mainWindow, tr("Export Property Set"),
                                 tr("There are no property definitions to export."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        m_mainWindow, tr("Export Property Definitions"),
        QDir::homePath() + QStringLiteral("/property_definitions.json"), tr(kJsonFilter));
    if (path.isEmpty()) return;

    QString err;
    if (!JsonFileIo::writeFile(path, PropertyDefinitionJson::toDocument(defs), err)) {
        QMessageBox::critical(m_mainWindow, tr("Export Failed"), err);
        return;
    }
    m_ui->statusbar->showMessage(
        tr("Exported %1 property definition(s) to %2").arg(defs.size()).arg(path), 5000);
}

void ConfigurationController::onImportPropertySet()
{
    const QString path = QFileDialog::getOpenFileName(
        m_mainWindow, tr("Import Property Definitions"),
        QDir::homePath(), tr(kJsonFilter));
    if (path.isEmpty()) return;

    QJsonDocument doc;
    QString err;
    if (!JsonFileIo::readFile(path, doc, err)) {
        QMessageBox::critical(m_mainWindow, tr("Import Failed"), err);
        return;
    }
    const auto imported = PropertyDefinitionJson::fromDocument(doc, err);
    if (imported.isEmpty()) {
        QMessageBox::critical(m_mainWindow, tr("Import Failed"),
                              err.isEmpty() ? tr("File contained no valid property definitions.") : err);
        return;
    }

    const int existing = m_propertyDefinitionModel->rowCount();
    bool append = false;
    if (existing > 0) {
        const auto reply = QMessageBox::question(
            m_mainWindow, tr("Import Property Set"),
            tr("Replace the %1 existing definition(s) with the imported ones, or append them?").arg(existing),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);
        if (reply == QMessageBox::Cancel) return;
        append = (reply == QMessageBox::No);
    }

    if (append) {
        for (const auto &def : imported)
            m_propertyDefinitionModel->addPropertyDefinition(def);
    } else {
        m_propertyDefinitionModel->setPropertyDefinitions(imported);
    }

    updatePropertyNamesCompleter();
    if (!m_deviceIdProvider().isEmpty()) {
        m_ui->statusbar->showMessage(tr("Loading current property values from device..."), 0);
        AdbManager::instance().fetchPropertyDefinitions(m_deviceIdProvider());
    }
    m_ui->statusbar->showMessage(tr("Imported %1 property definition(s).").arg(imported.size()), 3000);
}
