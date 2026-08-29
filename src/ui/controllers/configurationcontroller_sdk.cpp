#include "configurationcontroller.h"
#include "ui_mainwindow.h"
#include "tableconfig.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "propertydefinitionconverter.h"
#include "adbmanager.h"

#include <QHash>
#include <QMessageBox>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

// Helper: Find property by name
PropertyDefinition ConfigurationController::findPropertyByName(const QString &name) const
{
    for (const PropertyDefinition &propDef : m_availablePropertyDefinitions) {
        if (propDef.name.compare(name, Qt::CaseInsensitive) == 0)
            return propDef;
    }
    return PropertyDefinition();
}

// Helper: Parse property definition from ADB output
PropertyDefinition ConfigurationController::parsePropertyDefinition(
    const QString &output, const QString &fallbackName) const
{
    PropertyDefinition result;
    for (const QString &ln : output.split('\n', Qt::SkipEmptyParts)) {
        result = PropertyDefinitionConverter::convertLine(ln.trimmed());
        if (result.isValid()) break;
    }
    if (!result.isValid())
        result = PropertyDefinitionConverter::convertLine(output);
    if (result.isValid() && result.name.isEmpty())
        result.name = fallbackName;
    return result;
}

// Helper: Validate device ID
bool ConfigurationController::validateDeviceId(const QString &deviceId) const
{
    if (deviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, "No Device", "Please select a device first.");
        return false;
    }
    return true;
}

void ConfigurationController::onSearchPropertyDefinition()
{
    const QString searchText = m_ui->txtPropertySearch->text().trimmed();
    if (searchText.isEmpty()) return;

    const PropertyDefinition propDef = findPropertyByName(searchText);
    if (propDef.isValid()) {
        m_ui->statusbar->showMessage(
            QString("Found: %1 (ID: %2, Supported: %3)")
                .arg(propDef.name).arg(propDef.id)
                .arg(propDef.isSupported ? "Yes" : "No"),
            3000);
    } else {
        m_ui->statusbar->showMessage(QString("Property '%1' not found").arg(searchText), 3000);
    }
}

void ConfigurationController::onAddPropertyDefinition()
{
    const QString searchText = m_ui->txtPropertySearch->text().trimmed();
    if (searchText.isEmpty()) return;

    const PropertyDefinition selectedProp = findPropertyByName(searchText);
    if (!selectedProp.isValid()) {
        QMessageBox::warning(m_mainWindow, "Property Not Found",
                             QString("Property '%1' not found. Please fetch property definitions first.")
                                 .arg(searchText));
        return;
    }

    const int row = m_propertyDefinitionModel->rowCount();
    m_propertyDefinitionModel->addPropertyDefinition(selectedProp);

    if (m_propertyDefinitionModel->rowCount() > row) {
        m_ui->statusbar->showMessage(QString("Added property: %1").arg(selectedProp.name), 2000);
        m_ui->txtPropertySearch->clear();
    } else {
        m_ui->statusbar->showMessage(
            QString("Property '%1' already in list").arg(selectedProp.name), 2000);
    }
}

void ConfigurationController::onRefreshPropertyDefinitionValues()
{
    const QString deviceId = m_deviceIdProvider();
    if (!validateDeviceId(deviceId)) return;

    const QVector<PropertyDefinition> properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (properties.isEmpty()) {
        m_ui->statusbar->showMessage("No property definitions loaded.", 3000);
        return;
    }

    m_ui->btnFetchPropertyDefs->setEnabled(false);
    m_ui->statusbar->showMessage(
        QString("Refreshing values for %1 properties...").arg(properties.size()), 0);

    (void)QtConcurrent::run([this, properties, deviceId]() {
        using Pair = std::pair<int, PropertyDefinition>;
        QVector<Pair> results;
        results.reserve(properties.size());

        for (int row = 0; row < properties.size(); ++row) {
            const PropertyDefinition &propDef = properties[row];
            const QString queryKey = propDef.id.isEmpty() ? propDef.name : propDef.id;

            QString output, error;
            if (!AdbManager::instance().getPropertyDefinitionValue(deviceId, queryKey, output, error))
                continue;

            PropertyDefinition updated = parsePropertyDefinition(output, propDef.name);
            if (updated.isValid())
                results.append({row, updated});
        }

        QMetaObject::invokeMethod(this, [this, results]() {
            for (const auto &[row, updated] : results)
                m_propertyDefinitionModel->updatePropertyDefinition(row, updated);
            m_ui->btnFetchPropertyDefs->setEnabled(true);
            m_ui->statusbar->showMessage(
                QString("Refreshed values for %1 / %2 properties")
                    .arg(results.size())
                    .arg(m_propertyDefinitionModel->rowCount()),
                3000);
        }, Qt::QueuedConnection);
    });
}

void ConfigurationController::onFetchPropertyDefinitions()
{
    const QString deviceId = m_deviceIdProvider();
    if (!validateDeviceId(deviceId)) return;

    m_ui->statusbar->showMessage("Fetching property definitions...", 0);
    AdbManager::instance().fetchPropertyDefinitions(deviceId);
}

void ConfigurationController::onPropertyDefinitionsFetched(const QVector<PropertyDefinition> &defs)
{
    m_availablePropertyDefinitions = defs;
    updatePropertyNamesCompleter();

    QHash<QString, QString> valueByName;
    for (const PropertyDefinition &d : defs)
        valueByName.insert(d.name, d.value);

    const QVector<PropertyDefinition> &modelDefs = m_propertyDefinitionModel->getPropertyDefinitions();
    for (int i = 0; i < modelDefs.size(); ++i) {
        auto it = valueByName.constFind(modelDefs[i].name);
        if (it != valueByName.constEnd()) {
            PropertyDefinition updated = modelDefs[i];
            updated.value = it.value();
            m_propertyDefinitionModel->updatePropertyDefinition(i, updated);
        }
    }

    m_ui->statusbar->showMessage(
        QString("Fetched %1 property definitions").arg(defs.size()), 3000);
    m_propertyDefsMonitor.busy = false;
}

void ConfigurationController::onGetPropertyDefinitionClicked(int row)
{
    const QString deviceId = m_deviceIdProvider();
    if (!validateDeviceId(deviceId)) return;

    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size()) return;

    const PropertyDefinition &propDef = properties[row];
    const QString queryKey = propDef.id.isEmpty() ? propDef.name : propDef.id;
    QString output, error;

    if (!AdbManager::instance().getPropertyDefinitionValue(deviceId, queryKey, output, error)) {
        QMessageBox::warning(m_mainWindow, "Failed to Get Value",
                             QString("Failed to get %1:\n%2").arg(propDef.name, error));
        return;
    }

    PropertyDefinition updatedProp = parsePropertyDefinition(output, propDef.name);
    if (!updatedProp.isValid()) {
        QMessageBox::warning(m_mainWindow, "Parse Error",
                             QString("Failed to parse output for %1\n\nRaw output:\n%2")
                                 .arg(propDef.name, output.left(500)));
        return;
    }

    m_propertyDefinitionModel->updatePropertyDefinition(row, updatedProp);
    m_ui->statusbar->showMessage(QString("Updated property: %1").arg(updatedProp.name), 2000);
}

void ConfigurationController::onSetPropertyDefinitionClicked(int row)
{
    const QString deviceId = m_deviceIdProvider();
    if (!validateDeviceId(deviceId)) return;

    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size()) return;

    const PropertyDefinition &propDef = properties[row];
    if (propDef.readOnly) {
        QMessageBox::warning(m_mainWindow, "Read-Only Property",
                             QString("Property '%1' is marked as read-only.").arg(propDef.name));
        return;
    }

    using namespace TableConfig::PropertyDefColumns;
    QString value = m_propertyDefinitionModel->data(
                        m_propertyDefinitionModel->index(row, VALUE), Qt::DisplayRole).toString();
    value = value.replace("\"", "\\\"");

    QString error;
    if (AdbManager::instance().setPropertyDefinitionValue(deviceId, propDef.id, value, error))
        m_ui->statusbar->showMessage(QString("Set %1 = %2").arg(propDef.id, value), 3000);
    else
        QMessageBox::warning(m_mainWindow, "Failed to Set Value",
                             QString("Failed to set %1:\n%2").arg(propDef.id, error));
}

void ConfigurationController::onRemovePropertyDefinitionClicked(int row)
{
    const QVector<PropertyDefinition> &properties = m_propertyDefinitionModel->getPropertyDefinitions();
    if (row < 0 || row >= properties.size()) return;
    const PropertyDefinition prop = properties[row];
    m_propertyDefinitionModel->removePropertyDefinition(row);
    m_ui->statusbar->showMessage(QString("Removed property: %1").arg(prop.name), 2000);
}

void ConfigurationController::onClearAllPropertyDefinitions()
{
    if (m_propertyDefinitionModel->rowCount() == 0) {
        QMessageBox::information(m_mainWindow, "Empty List", "The property list is already empty.");
        return;
    }
    if (QMessageBox::question(m_mainWindow, "Clear All Properties",
            QString("Remove all %1 properties from the list?")
                .arg(m_propertyDefinitionModel->rowCount()),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
    {
        m_propertyDefinitionModel->clear();
        m_ui->statusbar->showMessage("Cleared all properties", 2000);
    }
}
