// ConfigurationController: property sets — saving, loading, exporting and
// importing configuration_manager values. Loading stages the values; nothing
// is written until the user applies them.
#include "configurationcontroller.h"
#include "ui_mainwindow.h"
#include "propertydefinitionmodel.h"
#include "presetdialogs.h"
#include "jsonfileio.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMessageBox>

namespace {
constexpr auto kJsonFilter = "JSON Files (*.json);;All Files (*)";
constexpr int  kMaxReportLines = 8;

QString reportLines(const QStringList &lines)
{
    QStringList shown = lines.mid(0, kMaxReportLines);
    if (lines.size() > kMaxReportLines)
        shown << QCoreApplication::translate("ConfigurationController", "… and %1 more")
                     .arg(lines.size() - kMaxReportLines);
    return shown.join(QStringLiteral("\n  "));
}
} // namespace

bool ConfigurationController::requirePropertyCatalog(const QString &title) const
{
    if (!m_propertyDefinitionModel->definitions().isEmpty())
        return true;
    QMessageBox::information(m_mainWindow, title,
                             tr("Load the property list from a device first: connect one and press Reload."));
    return false;
}

std::optional<QVector<PropertySetEntry>> ConfigurationController::choosePropertySet(const QString &title)
{
    enum Choice { Changed, Selected, Shown, Writable };
    const PropertyDefinitionModel &model = *m_propertyDefinitionModel;
    const auto entryFor = [&model](const PropertyDefinition &def) {
        return PropertySetEntry{def.name, def.id, def.type, model.shownValue(def)};
    };

    // Unavailable properties have no value worth keeping.
    QVector<PropertySetEntry> changed, selected, shown, writable;
    for (const PropertyDefinition &def : model.definitions()) {
        if (!def.available)
            continue;
        if (def.isWritable())
            writable.append(entryFor(def));
        if (!def.sameValue(model.shownValue(def), def.defaultValue))
            changed.append(entryFor(def));
    }
    for (int row = 0; row < model.rowCount(); ++row) {
        const PropertyDefinition *def = model.definitionAt(row);
        if (def && def->available)
            shown.append(entryFor(*def));
    }
    for (const QString &name : selectedPropertyNames()) {
        const PropertyDefinition *def = model.find(name);
        if (def && def->available)
            selected.append(entryFor(*def));
    }

    const QStringList options = {
        tr("Changed from default (staged values included) — %1").arg(changed.size()),
        tr("Selected rows — %1").arg(selected.size()),
        tr("Rows shown in the table — %1").arg(shown.size()),
        tr("All writable properties — %1").arg(writable.size()),
    };
    bool ok = false;
    const QString picked = QInputDialog::getItem(
        m_mainWindow, title, tr("Which property values?"), options,
        selected.size() > 1 ? Selected : Changed, false, &ok);
    if (!ok)
        return std::nullopt;

    QVector<PropertySetEntry> entries;
    switch (options.indexOf(picked)) {
    case Changed:  entries = changed;  break;
    case Selected: entries = selected; break;
    case Shown:    entries = shown;    break;
    case Writable: entries = writable; break;
    default:       return std::nullopt;
    }
    if (entries.isEmpty()) {
        QMessageBox::information(m_mainWindow, title, tr("That choice holds no property values."));
        return std::nullopt;
    }
    return entries;
}

void ConfigurationController::stagePropertySet(const QVector<PropertySetEntry> &entries,
                                               const QString &source)
{
    PropertyDefinitionModel &model = *m_propertyDefinitionModel;
    int staged = 0;
    int alreadySet = 0;
    QStringList unknown, notWritable, refused;

    for (const PropertySetEntry &entry : entries) {
        const PropertyDefinition *def = model.find(entry.name);
        if (!def && entry.id >= 0)
            def = model.findById(entry.id);   // renamed in another firmware
        if (!def) {
            unknown << entry.name;
            continue;
        }
        if (!def->isWritable()) {
            notWritable << def->name;
            continue;
        }
        if (def->sameValue(def->value, entry.value) && !model.isPending(def->name)) {
            ++alreadySet;
            continue;
        }
        const QString reason = model.stage(def->name, entry.value);
        if (reason.isEmpty())
            ++staged;
        else
            refused << reason;
    }

    QString text = staged == 1 ? tr("Staged 1 change from \"%1\".").arg(source)
                               : tr("Staged %1 changes from \"%2\".").arg(staged).arg(source);
    if (alreadySet > 0)
        text += QLatin1Char('\n') + tr("%1 value(s) already match the device.").arg(alreadySet);
    if (!unknown.isEmpty())
        text += QStringLiteral("\n\n") + tr("Not on this device:") + QStringLiteral("\n  ") + reportLines(unknown);
    if (!notWritable.isEmpty())
        text += QStringLiteral("\n\n") + tr("Read-only or unavailable, skipped:")
                + QStringLiteral("\n  ") + reportLines(notWritable);
    if (!refused.isEmpty())
        text += QStringLiteral("\n\n") + tr("Refused:") + QStringLiteral("\n  ") + reportLines(refused);

    if (staged > 0) {
        if (m_ui->chkApplyPropertiesImmediately->isChecked()) {
            text += QStringLiteral("\n\n") + tr("\"Apply immediately\" is on: they are being written now.");
        } else {
            text += QStringLiteral("\n\n") + tr("Review them in the table, then press Apply to write them.");
            // Show exactly what is about to change.
            const int pendingScope = m_ui->cmbPropertyScope->findData(int(PropertyDefinitionModel::Scope::Pending));
            if (pendingScope >= 0)
                m_ui->cmbPropertyScope->setCurrentIndex(pendingScope);
        }
    }
    QMessageBox::information(m_mainWindow, tr("Property Set"), text);
}

void ConfigurationController::onSavePropertySet()
{
    const QString title = tr("Save Property Set");
    if (!requirePropertyCatalog(title))
        return;
    const auto entries = choosePropertySet(title);
    if (!entries)
        return;

    const QString name = PresetDialogs::askPresetName(
        m_mainWindow, title, tr("Name for these %1 property values:").arg(entries->size()));
    if (name.isEmpty())
        return;

    QString err;
    if (!m_propDefStore.savePreset(name, PropertySetJson::toBytes(*entries), err)) {
        QMessageBox::critical(m_mainWindow, tr("Save Failed"), err);
        return;
    }
    m_ui->statusbar->showMessage(tr("Property set \"%1\" saved.").arg(name), 3000);
}

void ConfigurationController::onLoadPropertySet()
{
    const QString title = tr("Load Property Set");
    if (!requirePropertyCatalog(title))
        return;
    const QString name = PresetDialogs::pickPresetWithDelete(
        m_mainWindow, title, tr("Stage the values of a saved property set:"), m_propDefStore);
    if (name.isEmpty())
        return;

    QString err;
    const QVector<PropertySetEntry> entries = PropertySetJson::fromBytes(m_propDefStore.loadPreset(name), err);
    if (!err.isEmpty()) {
        QMessageBox::critical(m_mainWindow, tr("Load Failed"), err);
        return;
    }
    stagePropertySet(entries, name);
}

void ConfigurationController::onExportPropertySet()
{
    const QString title = tr("Export Property Set");
    if (!requirePropertyCatalog(title))
        return;
    const auto entries = choosePropertySet(title);
    if (!entries)
        return;

    const QString path = QFileDialog::getSaveFileName(
        m_mainWindow, title, QDir::homePath() + QStringLiteral("/property-set.json"), tr(kJsonFilter));
    if (path.isEmpty())
        return;

    QString err;
    if (!JsonFileIo::writeFile(path, PropertySetJson::toDocument(*entries), err)) {
        QMessageBox::critical(m_mainWindow, tr("Export Failed"), err);
        return;
    }
    m_ui->statusbar->showMessage(tr("Exported %1 property value(s) to %2").arg(entries->size()).arg(path), 5000);
}

void ConfigurationController::onImportPropertySet()
{
    const QString title = tr("Import Property Set");
    if (!requirePropertyCatalog(title))
        return;
    const QString path = QFileDialog::getOpenFileName(m_mainWindow, title, QDir::homePath(), tr(kJsonFilter));
    if (path.isEmpty())
        return;

    QJsonDocument doc;
    QString err;
    if (!JsonFileIo::readFile(path, doc, err)) {
        QMessageBox::critical(m_mainWindow, tr("Import Failed"), err);
        return;
    }
    const QVector<PropertySetEntry> entries = PropertySetJson::fromDocument(doc, err);
    if (entries.isEmpty()) {
        QMessageBox::critical(m_mainWindow, tr("Import Failed"),
                              err.isEmpty() ? tr("The file holds no property values.") : err);
        return;
    }
    stagePropertySet(entries, QFileInfo(path).fileName());
}
