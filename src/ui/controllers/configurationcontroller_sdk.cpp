// ConfigurationController: the SDK tab's configuration_manager property table —
// loading, filtering, staging, applying and resetting property values.
#include "configurationcontroller.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "propertydefinitionmodel.h"
#include "propertyvaluedelegate.h"
#include "tableconfig.h"
#include "tablestyler.h"
#include "tooltips.h"
#include "components/components.h"

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QStyle>
#include <QTimer>

#include <utility>

namespace {

using Scope = PropertyDefinitionModel::Scope;

constexpr int  kFilterDebounceMs   = 150;
constexpr int  kStatusMessageMs    = 4000;
constexpr int  kMaxListedLines     = 12;
constexpr auto kFilterSettingKey   = "sdk/propertyFilter";
constexpr auto kScopeSettingKey    = "sdk/propertyScope";
constexpr auto kApplyNowSettingKey = "sdk/applyImmediately";

QString trc(const char *text)
{
    return QCoreApplication::translate("ConfigurationController", text);
}

/** "1 value" / "3 values", without relying on a plural translation file. */
QString countText(qsizetype n, const char *one, const char *many)
{
    return n == 1 ? trc(one) : trc(many).arg(n);
}

/** Up to kMaxListedLines lines, then how many more there are. */
QString listLines(const QStringList &lines)
{
    QStringList shown = lines.mid(0, kMaxListedLines);
    if (lines.size() > kMaxListedLines)
        shown << trc("… and %1 more").arg(lines.size() - kMaxListedLines);
    return shown.join(QLatin1Char('\n'));
}

/** Warning colours come from the theme sheet's [state="warning"] rules. */
void setWarningState(QWidget *widget, bool warning)
{
    const QVariant state = warning ? QVariant(QStringLiteral("warning")) : QVariant();
    if (!widget || widget->property("state") == state)
        return;
    widget->setProperty("state", state);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

bool ConfigurationController::validateDeviceId(const QString &deviceId) const
{
    if (deviceId.isEmpty()) {
        QMessageBox::warning(m_mainWindow, tr("No Device"), tr("Please select a device first."));
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Setup
// ─────────────────────────────────────────────────────────────────────────────

void ConfigurationController::setupSDKTab()
{
    using namespace TableConfig::PropertyDefColumns;
    using namespace TableConfig::ColumnWidths;
    using UiComponents::Button;
    using UiComponents::ButtonSize;
    using UiComponents::ButtonVariant;

    auto *table = m_ui->tablePropertyDefinitions;
    table->setModel(m_propertyDefinitionModel);
    TableStyler::applyConfigTableStyle({table});
    // Hundreds of one-line rows: fixed heights keep scrolling and refreshes cheap.
    table->setWordWrap(false);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                           | QAbstractItemView::SelectedClicked);

    QHeaderView *header = table->horizontalHeader();
    header->setStretchLastSection(false);
    table->setColumnWidth(NAME,    PROPDEF_NAME);
    table->setColumnWidth(DEFAULT, PROPDEF_DEFAULT);
    table->setColumnWidth(TYPE,    PROPDEF_TYPE);
    table->setColumnWidth(ID,      PROPDEF_ID);
    table->setColumnWidth(NOTES,   PROPDEF_NOTES);
    header->setSectionResizeMode(VALUE, QHeaderView::Stretch);
    header->setSortIndicator(NAME, Qt::AscendingOrder);
    table->setSortingEnabled(true);

    m_propertyValueDelegate = new PropertyValueDelegate(this);
    table->setItemDelegate(m_propertyValueDelegate);
    connect(m_propertyValueDelegate, &PropertyValueDelegate::valueRejected, this,
            [this](const QString &reason) { m_ui->statusbar->showMessage(reason, kStatusMessageMs); });

    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QTableView::customContextMenuRequested,
            this, &ConfigurationController::showPropertyContextMenu);
    connect(table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() { updatePropertyActions(); });
    connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this]() { updatePropertyDetails(); });

    // ── Filter box and scope ─────────────────────────────────────────────────
    m_ui->txtPropertySearch->setClearButtonEnabled(true);
    m_ui->txtPropertySearch->setToolTip(tr(Tooltips::txtPropertySearch));
    m_propertyFilterTimer = new QTimer(this);
    m_propertyFilterTimer->setSingleShot(true);
    m_propertyFilterTimer->setInterval(kFilterDebounceMs);
    connect(m_propertyFilterTimer, &QTimer::timeout, this, &ConfigurationController::applyPropertyFilterText);
    connect(m_ui->txtPropertySearch, &QLineEdit::textChanged,
            m_propertyFilterTimer, qOverload<>(&QTimer::start));
    connect(m_ui->txtPropertySearch, &QLineEdit::returnPressed, this, [this]() {
        m_propertyFilterTimer->stop();
        applyPropertyFilterText();
    });

    QComboBox *scope = m_ui->cmbPropertyScope;
    scope->setToolTip(tr(Tooltips::cmbPropertyScope));
    scope->addItem(tr("All properties"),       int(Scope::All));
    scope->addItem(tr("Changed from default"), int(Scope::Modified));
    scope->addItem(tr("Staged changes"),       int(Scope::Pending));
    scope->addItem(tr("Writable"),             int(Scope::Writable));
    scope->addItem(tr("Read-only"),            int(Scope::ReadOnly));
    scope->addItem(tr("Needs reboot"),         int(Scope::NeedsReboot));
    scope->addItem(tr("Unavailable"),          int(Scope::Unavailable));
    connect(scope, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, scope](int) {
        m_propertyDefinitionModel->setScope(Scope(scope->currentData().toInt()));
        QSettings().setValue(QLatin1String(kScopeSettingKey), scope->currentData().toInt());
    });
    m_ui->lblPropertyFilterStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // ── Change bar ───────────────────────────────────────────────────────────
    Button::style(m_ui->btnApplyPropertyChanges,   ButtonVariant::Primary,   ButtonSize::Small);
    Button::style(m_ui->btnDiscardPropertyChanges, ButtonVariant::Ghost,     ButtonSize::Small);
    Button::style(m_ui->btnResetPropertyDefaults,  ButtonVariant::Secondary, ButtonSize::Small);
    m_ui->btnApplyPropertyChanges->setToolTip(tr(Tooltips::btnApplyPropertyChanges));
    m_ui->btnDiscardPropertyChanges->setToolTip(tr(Tooltips::btnDiscardPropertyChanges));
    m_ui->btnResetPropertyDefaults->setToolTip(tr(Tooltips::btnResetPropertyDefaults));
    m_ui->chkApplyPropertiesImmediately->setToolTip(tr(Tooltips::chkApplyPropertiesImmediately));
    m_ui->lblPropertyDetails->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_autoApplyTimer = new QTimer(this);
    m_autoApplyTimer->setSingleShot(true);
    m_autoApplyTimer->setInterval(0);
    connect(m_autoApplyTimer, &QTimer::timeout, this, [this]() { applyPropertyChanges(false); });

    connect(m_ui->btnApplyPropertyChanges, &QPushButton::clicked,
            this, [this]() { applyPropertyChanges(true); });
    connect(m_ui->btnDiscardPropertyChanges, &QPushButton::clicked,
            this, &ConfigurationController::discardPropertyChanges);
    connect(m_ui->btnResetPropertyDefaults, &QPushButton::clicked,
            this, [this]() { resetPropertiesToDefault(selectedPropertyNames()); });
    connect(m_ui->chkApplyPropertiesImmediately, &QCheckBox::toggled, this, [this](bool on) {
        QSettings().setValue(QLatin1String(kApplyNowSettingKey), on);
        if (on)
            m_autoApplyTimer->start();
    });

    auto *applyShortcut = new QShortcut(QKeySequence::Save, m_ui->tabSDKConfiguration);
    applyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(applyShortcut, &QShortcut::activated, this, [this]() { applyPropertyChanges(true); });

    // ── Toolbar ──────────────────────────────────────────────────────────────
    connect(m_ui->btnFetchPropertyDefs, &QPushButton::clicked, this, [this]() {
        if (validateDeviceId(m_deviceIdProvider()))
            refreshPropertyDefinitions();
    });
    connect(m_ui->btnSavePropertySet,   &QPushButton::clicked, this, &ConfigurationController::onSavePropertySet);
    connect(m_ui->btnLoadPropertySet,   &QPushButton::clicked, this, &ConfigurationController::onLoadPropertySet);
    connect(m_ui->btnExportPropertySet, &QPushButton::clicked, this, &ConfigurationController::onExportPropertySet);
    connect(m_ui->btnImportPropertySet, &QPushButton::clicked, this, &ConfigurationController::onImportPropertySet);
    m_ui->btnSavePropertySet->setToolTip(tr("Save property values as a named set"));
    m_ui->btnLoadPropertySet->setToolTip(tr("Stage the values of a saved property set"));
    m_ui->btnExportPropertySet->setToolTip(tr("Export property values to a JSON file"));
    m_ui->btnImportPropertySet->setToolTip(tr("Stage the values of a property set JSON file"));

    // ── Model feedback ───────────────────────────────────────────────────────
    connect(m_propertyDefinitionModel, &PropertyDefinitionModel::pendingChanged, this, [this]() {
        updatePropertyActions();
        updatePropertySummary();
        if (m_ui->chkApplyPropertiesImmediately->isChecked())
            m_autoApplyTimer->start();
    });
    connect(m_propertyDefinitionModel, &QAbstractItemModel::modelReset, this, [this]() {
        updatePropertyFilterStatus();
        updatePropertyDetails();
        updatePropertyActions();
    });
    connect(m_propertyDefinitionModel, &QAbstractItemModel::dataChanged,
            this, [this]() { updatePropertyDetails(); });

    // ── Restore the last filter, scope and apply mode ────────────────────────
    const QSettings settings;
    {
        const QSignalBlocker blockText(m_ui->txtPropertySearch);
        m_ui->txtPropertySearch->setText(settings.value(QLatin1String(kFilterSettingKey)).toString());
    }
    m_propertyDefinitionModel->setFilter(m_ui->txtPropertySearch->text());
    const int savedScope = scope->findData(settings.value(QLatin1String(kScopeSettingKey), int(Scope::All)));
    scope->setCurrentIndex(qMax(0, savedScope));
    {
        const QSignalBlocker blockCheck(m_ui->chkApplyPropertiesImmediately);
        m_ui->chkApplyPropertiesImmediately->setChecked(
            settings.value(QLatin1String(kApplyNowSettingKey), false).toBool());
    }

    updatePropertySummary();
    updatePropertyFilterStatus();
    updatePropertyDetails();
    updatePropertyActions();
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Loading
// ─────────────────────────────────────────────────────────────────────────────

void ConfigurationController::refreshPropertyDefinitions()
{
    const QString deviceId = m_deviceIdProvider();
    if (deviceId.isEmpty() || m_propertyFetchInFlight)
        return;
    m_propertyFetchInFlight = true;
    m_ui->btnFetchPropertyDefs->setEnabled(false);
    updatePropertySummary();
    AdbManager::instance().fetchPropertyDefinitions(deviceId);
}

void ConfigurationController::clearPropertyDefinitions()
{
    m_propertyDeviceId.clear();
    m_propertyLoadError.clear();
    m_propertyRefetch = false;
    m_propertyDefinitionModel->clear();
    updatePropertySummary();
    updatePropertyActions();
}

void ConfigurationController::onPropertyDefinitionsFetched(const QString &deviceId,
                                                           const QVector<PropertyDefinition> &definitions,
                                                           const QString &error)
{
    m_propertyDefsMonitor.busy = false;
    m_propertyFetchInFlight = false;
    m_ui->btnFetchPropertyDefs->setEnabled(true);

    // A write finished while this load ran, or the device changed under it:
    // what arrived is already stale.
    if (m_propertyRefetch || deviceId != m_deviceIdProvider()) {
        m_propertyRefetch = false;
        refreshPropertyDefinitions();
        return;
    }

    if (!error.isEmpty()) {
        m_propertyLoadError = error;
        if (m_propertyDeviceId != deviceId)
            clearPropertyDefinitions();
        m_propertyLoadError = error;
        if (!m_propertyDefsMonitor.active) {
            m_ui->statusbar->showMessage(
                tr("Could not load properties: %1").arg(error.section(QLatin1Char('\n'), 0, 0)),
                kStatusMessageMs);
        }
        updatePropertySummary();
        return;
    }

    m_propertyLoadError.clear();
    if (m_propertyDeviceId != deviceId) {
        // Staged values belong to the device they were made for.
        const int dropped = m_propertyDefinitionModel->pendingCount();
        m_propertyDefinitionModel->clear();
        if (dropped > 0) {
            m_ui->statusbar->showMessage(
                countText(dropped, "Dropped 1 staged change made for the previous device.",
                          "Dropped %1 staged changes made for the previous device."),
                kStatusMessageMs);
        }
        m_propertyDeviceId = deviceId;
    }
    m_propertyDefinitionModel->setDefinitions(definitions);

    updatePropertySummary();
    updatePropertyFilterStatus();
    updatePropertyActions();
    updatePropertyDetails();
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Writing
// ─────────────────────────────────────────────────────────────────────────────

void ConfigurationController::applyPropertyChanges(bool retryFailed)
{
    if (m_propertyWriteInFlight)
        return;
    const QVector<QPair<QString, QString>> changes =
        m_propertyDefinitionModel->pendingChanges(retryFailed);
    if (changes.isEmpty())
        return;

    const QString deviceId = m_deviceIdProvider();
    if (deviceId.isEmpty() || deviceId != m_propertyDeviceId) {
        if (retryFailed) {
            QMessageBox::warning(m_mainWindow, tr("Apply Changes"),
                                 tr("Connect the device these changes were made for, then apply them."));
        }
        return;
    }

    m_propertyWriteValues.clear();
    for (const auto &[name, value] : changes)
        m_propertyWriteValues.insert(name, value);
    m_propertyWriteInFlight = true;
    updatePropertyActions();
    m_ui->statusbar->showMessage(countText(changes.size(), "Writing 1 value…", "Writing %1 values…"));
    AdbManager::instance().writePropertyDefinitions(deviceId, changes);
}

void ConfigurationController::resetPropertiesToDefault(const QStringList &names)
{
    if (m_propertyWriteInFlight)
        return;

    QStringList resettable;
    QStringList lines;
    for (const QString &name : names) {
        const PropertyDefinition *def = m_propertyDefinitionModel->find(name);
        if (!def || !def->isWritable()
            || (!def->isModified() && !m_propertyDefinitionModel->isPending(name)))
            continue;
        resettable << name;
        lines << QStringLiteral("%1:  %2  →  %3")
                     .arg(name, m_propertyDefinitionModel->shownValue(*def), def->defaultValue);
    }
    if (resettable.isEmpty()) {
        m_ui->statusbar->showMessage(
            tr("The selected properties already hold their defaults, or cannot be written."),
            kStatusMessageMs);
        return;
    }

    const QString deviceId = m_deviceIdProvider();
    if (!validateDeviceId(deviceId) || deviceId != m_propertyDeviceId)
        return;

    const auto answer = QMessageBox::question(
        m_mainWindow, tr("Reset to Default"),
        tr("Reset %1 to the default value on the device?\n\n%2")
            .arg(countText(resettable.size(), "1 property", "%1 properties"), listLines(lines)),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    // The reset replaces whatever was staged for these properties.
    m_propertyDefinitionModel->unstage(resettable);
    m_propertyWriteValues.clear();
    for (const QString &name : std::as_const(resettable))
        m_propertyWriteValues.insert(name, QString());
    m_propertyWriteInFlight = true;
    updatePropertyActions();
    m_ui->statusbar->showMessage(
        countText(resettable.size(), "Resetting 1 property…", "Resetting %1 properties…"));
    AdbManager::instance().resetPropertyDefinitions(deviceId, resettable);
}

void ConfigurationController::onPropertyDefinitionsWritten(const QString &deviceId, bool reset,
                                                           const PropertyWriteResult &result)
{
    m_propertyWriteInFlight = false;
    const QHash<QString, QString> attempted = std::exchange(m_propertyWriteValues, {});
    const bool sameDevice = deviceId == m_propertyDeviceId && deviceId == m_deviceIdProvider();

    QStringList rebootNames;
    if (sameDevice) {
        for (const QString &name : result.done) {
            const PropertyDefinition *def = m_propertyDefinitionModel->find(name);
            if (def && def->needReboot)
                rebootNames << name;
        }
        if (!reset) {
            QVector<QPair<QString, QString>> applied;
            for (const QString &name : result.done) {
                const auto it = attempted.constFind(name);
                if (it != attempted.constEnd())
                    applied.append({name, it.value()});
            }
            m_propertyDefinitionModel->markApplied(applied);
            for (const auto &[name, reason] : result.failed)
                m_propertyDefinitionModel->setWriteError(name, reason);
            if (!result.error.isEmpty()) {
                for (auto it = attempted.cbegin(); it != attempted.cend(); ++it) {
                    if (!result.done.contains(it.key()))
                        m_propertyDefinitionModel->setWriteError(it.key(), result.error);
                }
            }
        }

        // Writing one property can move others: read every value back.
        if (m_propertyFetchInFlight)
            m_propertyRefetch = true;
        else
            refreshPropertyDefinitions();
    }
    updatePropertyActions();
    updatePropertySummary();

    if (result.succeeded()) {
        m_ui->statusbar->showMessage(
            reset ? countText(result.done.size(), "Reset 1 property to its default.",
                              "Reset %1 properties to their defaults.")
                  : countText(result.done.size(), "Wrote 1 value to the device.",
                              "Wrote %1 values to the device."),
            kStatusMessageMs);
    } else {
        QStringList lines;
        for (const auto &[name, reason] : result.failed)
            lines << QStringLiteral("%1 — %2").arg(name, reason);
        QString text = (reset ? tr("%1 of %2 properties were reset.") : tr("%1 of %2 values were written."))
                           .arg(result.done.size()).arg(attempted.size());
        if (!lines.isEmpty())
            text += QStringLiteral("\n\n") + listLines(lines);
        if (!result.error.isEmpty())
            text += QStringLiteral("\n\n") + result.error;
        if (!reset)
            text += QStringLiteral("\n\n")
                    + tr("Values that were not written stay staged; hover one to see why.");
        m_ui->statusbar->clearMessage();
        QMessageBox::warning(m_mainWindow, reset ? tr("Reset Incomplete") : tr("Write Incomplete"), text);
    }

    if (!rebootNames.isEmpty()) {
        const auto answer = QMessageBox::question(
            m_mainWindow, tr("Reboot Needed"),
            tr("These changes take effect after the device reboots:\n\n%1\n\nReboot the device now?")
                .arg(listLines(rebootNames)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes)
            AdbManager::instance().runRawAdbCommand(QStringLiteral("adb -s %1 reboot").arg(deviceId));
    }

    if (sameDevice && m_ui->chkApplyPropertiesImmediately->isChecked())
        m_autoApplyTimer->start();
}

void ConfigurationController::discardPropertyChanges()
{
    const int count = m_propertyDefinitionModel->pendingCount();
    if (count == 0)
        return;
    if (count > 1) {
        const auto answer = QMessageBox::question(
            m_mainWindow, tr("Discard Changes"),
            tr("Discard all %1 staged changes? Nothing has been written to the device yet.").arg(count),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
    }
    m_propertyDefinitionModel->discardAll();
    m_ui->statusbar->showMessage(countText(count, "Discarded 1 staged change.",
                                           "Discarded %1 staged changes."),
                                 kStatusMessageMs);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Row actions
// ─────────────────────────────────────────────────────────────────────────────

QStringList ConfigurationController::selectedPropertyNames() const
{
    QModelIndexList rows = m_ui->tablePropertyDefinitions->selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(),
              [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
    QStringList names;
    for (const QModelIndex &index : std::as_const(rows)) {
        if (const PropertyDefinition *def = m_propertyDefinitionModel->definitionAt(index.row()))
            names << def->name;
    }
    return names;
}

void ConfigurationController::editPropertyValue(const QString &name)
{
    const PropertyDefinition *def = m_propertyDefinitionModel->find(name);
    if (!def)
        return;
    if (!def->isWritable()) {
        m_ui->statusbar->showMessage(def->validate(QString(), nullptr), kStatusMessageMs);
        return;
    }

    const QString current = m_propertyDefinitionModel->shownValue(*def);
    switch (def->kind()) {
    case PropertyDefinition::Kind::Boolean:
        m_propertyDefinitionModel->stage(
            name, def->sameValue(current, QStringLiteral("true")) ? QStringLiteral("false")
                                                                  : QStringLiteral("true"));
        return;
    case PropertyDefinition::Kind::String:
    case PropertyDefinition::Kind::Blob:
    case PropertyDefinition::Kind::Other: {
        // Long text and serialized blobs are easier to edit with room to see them.
        bool ok = false;
        const QString text = QInputDialog::getMultiLineText(
            m_mainWindow, tr("Edit %1").arg(name), def->typeSummary(), current, &ok);
        if (!ok)
            return;
        const QString reason = m_propertyDefinitionModel->stage(name, text);
        if (!reason.isEmpty())
            QMessageBox::warning(m_mainWindow, tr("Edit %1").arg(name), reason);
        return;
    }
    default: {
        const int row = m_propertyDefinitionModel->rowOf(name);
        if (row < 0)
            return;
        const QModelIndex index = m_propertyDefinitionModel->index(row, TableConfig::PropertyDefColumns::VALUE);
        m_ui->tablePropertyDefinitions->setCurrentIndex(index);
        m_ui->tablePropertyDefinitions->edit(index);
        return;
    }
    }
}

void ConfigurationController::showPropertyContextMenu(const QPoint &pos)
{
    QTableView *table = m_ui->tablePropertyDefinitions;
    const QModelIndex clicked = table->indexAt(pos);
    if (clicked.isValid() && !table->selectionModel()->isRowSelected(clicked.row(), QModelIndex())) {
        table->selectionModel()->setCurrentIndex(
            clicked, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }

    const QStringList names = selectedPropertyNames();
    if (names.isEmpty())
        return;
    const PropertyDefinition *first = m_propertyDefinitionModel->find(names.first());
    if (!first)
        return;

    bool anyPending = false;
    bool anyResettable = false;
    for (const QString &name : names) {
        const PropertyDefinition *def = m_propertyDefinitionModel->find(name);
        const bool pending = m_propertyDefinitionModel->isPending(name);
        anyPending = anyPending || pending;
        anyResettable = anyResettable || (def && def->isWritable() && (def->isModified() || pending));
    }
    const bool busy = m_propertyWriteInFlight;

    QMenu menu(table);
    QAction *edit = menu.addAction(tr("Edit Value…"));
    edit->setEnabled(names.size() == 1 && first->isWritable());
    QAction *revert = menu.addAction(tr("Revert Staged Change"));
    revert->setEnabled(anyPending);
    QAction *stageDefault = menu.addAction(tr("Stage Default Value"));
    stageDefault->setEnabled(anyResettable);
    QAction *reset = menu.addAction(tr("Reset to Default on Device…"));
    reset->setEnabled(anyResettable && !busy);
    menu.addSeparator();
    QAction *copyNames   = menu.addAction(names.size() == 1 ? tr("Copy Name") : tr("Copy Names"));
    QAction *copyValues  = menu.addAction(tr("Copy as NAME = value"));
    QAction *copyCommand = menu.addAction(tr("Copy as adb Command"));
    menu.addSeparator();
    QAction *onlyType = menu.addAction(tr("Filter to Type \"%1\"").arg(first->type));

    const QAction *chosen = menu.exec(table->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == edit) {
        editPropertyValue(first->name);
    } else if (chosen == revert) {
        m_propertyDefinitionModel->unstage(names);
    } else if (chosen == stageDefault) {
        for (const QString &name : names) {
            const PropertyDefinition *def = m_propertyDefinitionModel->find(name);
            if (def && def->isWritable())
                m_propertyDefinitionModel->stage(name, def->defaultValue);
        }
    } else if (chosen == reset) {
        resetPropertiesToDefault(names);
    } else if (chosen == copyNames || chosen == copyValues || chosen == copyCommand) {
        QStringList lines;
        QStringList pairs;
        for (const QString &name : names) {
            const PropertyDefinition *def = m_propertyDefinitionModel->find(name);
            if (!def)
                continue;
            const QString value = m_propertyDefinitionModel->shownValue(*def);
            lines << (chosen == copyNames ? name : QStringLiteral("%1 = %2").arg(name, value));
            if (def->available)
                pairs << name << ConfigurationManagerOutput::shellQuote(value);
        }
        QString text = lines.join(QLatin1Char('\n'));
        if (chosen == copyCommand) {
            // Double quotes carry the whole command, single-quoted values
            // included, through the host shell to the device shell.
            const QString deviceId = m_deviceIdProvider();
            text = QStringLiteral("adb %1shell \"cmd configuration_manager set %2\"")
                       .arg(deviceId.isEmpty() ? QString() : QStringLiteral("-s %1 ").arg(deviceId),
                            pairs.join(QLatin1Char(' ')));
        }
        QApplication::clipboard()->setText(text);
        m_ui->statusbar->showMessage(tr("Copied to the clipboard."), kStatusMessageMs);
    } else if (chosen == onlyType) {
        const FilterQuery::Edit filter = FilterQuery::includeTerm(
            m_ui->txtPropertySearch->text(), PropertyDefinitionQuery::schema(),
            int(PropertyDefinitionQuery::Field::Type), first->type);
        m_ui->txtPropertySearch->setText(filter.text);
        m_propertyFilterTimer->stop();
        applyPropertyFilterText();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Status
// ─────────────────────────────────────────────────────────────────────────────

void ConfigurationController::applyPropertyFilterText()
{
    const QString text = m_ui->txtPropertySearch->text();
    m_propertyDefinitionModel->setFilter(text);
    QSettings().setValue(QLatin1String(kFilterSettingKey), text);
}

void ConfigurationController::updatePropertyActions()
{
    const int pending = m_propertyDefinitionModel->pendingCount();
    const bool busy = m_propertyWriteInFlight;
    m_ui->btnApplyPropertyChanges->setText(pending > 0 ? tr("Apply (%1)").arg(pending) : tr("Apply"));
    m_ui->btnApplyPropertyChanges->setEnabled(pending > 0 && !busy);
    m_ui->btnDiscardPropertyChanges->setEnabled(pending > 0 && !busy);

    bool resettable = false;
    for (const QString &name : selectedPropertyNames()) {
        const PropertyDefinition *def = m_propertyDefinitionModel->find(name);
        if (def && def->isWritable()
            && (def->isModified() || m_propertyDefinitionModel->isPending(name))) {
            resettable = true;
            break;
        }
    }
    m_ui->btnResetPropertyDefaults->setEnabled(resettable && !busy);
}

void ConfigurationController::updatePropertySummary()
{
    QLabel *label = m_ui->lblPropertyDefsSummary;
    const qsizetype total = m_propertyDefinitionModel->definitions().size();

    QString text;
    QString tip;
    bool warning = false;
    if (!m_propertyLoadError.isEmpty()) {
        text = QStringLiteral("⚠ ") + m_propertyLoadError.section(QLatin1Char('\n'), 0, 0);
        tip = m_propertyLoadError;
        warning = true;
    } else if (total == 0) {
        text = m_propertyFetchInFlight        ? tr("Loading properties…")
               : m_deviceIdProvider().isEmpty() ? tr("No device connected")
                                                : tr("Not loaded — press Reload");
    } else {
        QStringList parts;
        parts << countText(total, "1 property", "%1 properties");
        parts << tr("%1 changed from default").arg(m_propertyDefinitionModel->modifiedCount());
        if (const int unavailable = m_propertyDefinitionModel->unavailableCount())
            parts << tr("%1 unavailable").arg(unavailable);
        if (const int pending = m_propertyDefinitionModel->pendingCount())
            parts << tr("%1 staged").arg(pending);
        text = parts.join(QStringLiteral("  ·  "));
    }
    label->setText(text);
    label->setToolTip(tip);
    setWarningState(label, warning);
}

void ConfigurationController::updatePropertyFilterStatus()
{
    QLabel *label = m_ui->lblPropertyFilterStatus;
    const qsizetype total = m_propertyDefinitionModel->definitions().size();
    const int shown = m_propertyDefinitionModel->rowCount();
    const PropertyDefinitionQuery &query = m_propertyDefinitionModel->filterQuery();
    const bool narrowed = !query.isEmpty() || m_propertyDefinitionModel->scope() != Scope::All;

    QString text = narrowed ? tr("%1 / %2").arg(shown).arg(total)
                            : countText(total, "1 row", "%1 rows");
    const bool warning = !query.warning().isEmpty();
    if (warning)
        text.prepend(QStringLiteral("⚠ "));
    label->setText(text);
    // The box's own tooltip is the syntax help, so the repair note goes here.
    label->setToolTip(warning ? query.warning() : QString());
    setWarningState(label, warning);
    setWarningState(m_ui->txtPropertySearch, warning);
}

void ConfigurationController::updatePropertyDetails()
{
    QLabel *label = m_ui->lblPropertyDetails;
    const QModelIndex current = m_ui->tablePropertyDefinitions->currentIndex();
    const PropertyDefinition *def =
        current.isValid() ? m_propertyDefinitionModel->definitionAt(current.row()) : nullptr;
    if (!def) {
        label->setText(tr("Double-click a value to edit it. Changes stay staged until you apply them."));
        return;
    }

    QStringList parts;
    parts << def->name << tr("id %1").arg(def->id) << def->typeSummary();
    parts << tr("default %1").arg(def->defaultValue.isEmpty() ? tr("(empty)") : def->defaultValue);
    if (!def->available)
        parts << tr("not available on this device");
    else if (def->readOnly)
        parts << tr("read-only");
    if (def->needReboot)
        parts << tr("takes effect after a reboot");
    label->setText(parts.join(QStringLiteral("  ·  ")));
}
