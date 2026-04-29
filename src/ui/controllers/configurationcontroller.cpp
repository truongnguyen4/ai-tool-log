#include "configurationcontroller.h"
#include "ui_mainwindow.h"
#include "tableconfig.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "blinkdelegate.h"
#include "colorscheme.h"
#include "widgetstyling.h"
#include "adbmanager.h"

#include <QCompleter>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QStringListModel>
#include <QTimer>

#include "components/components.h"
#include <QStyle>
#include <QTableView>
#include <QtConcurrent>

ConfigurationController::ConfigurationController(Ui::MainWindow *ui,
                                                 QMainWindow *mainWindow,
                                                 SettingsModel *settingsModel,
                                                 PropertiesModel *propertiesModel,
                                                 PropertyDefinitionModel *propertyDefinitionModel,
                                                 DeviceIdProvider deviceIdProvider,
                                                 QObject *parent)
    : QObject(parent)
    , m_ui(ui)
    , m_mainWindow(mainWindow)
    , m_settingsModel(settingsModel)
    , m_propertiesModel(propertiesModel)
    , m_propertyDefinitionModel(propertyDefinitionModel)
    , m_deviceIdProvider(std::move(deviceIdProvider))
{
}

void ConfigurationController::setupTables()
{
    using namespace TableConfig::SettingsColumns;
    using namespace TableConfig::ColumnWidths;

    // ── Settings table ────────────────────────────────────────────────────────
    m_ui->tableSettings->setModel(m_settingsModel);
    m_ui->tableSettings->horizontalHeader()->setStretchLastSection(false);
    m_ui->tableSettings->setColumnWidth(LINE,    SETTINGS_LINE);
    m_ui->tableSettings->setColumnWidth(GROUP,   SETTINGS_GROUP);
    m_ui->tableSettings->setColumnWidth(SETTING, SETTINGS_SETTING);
    m_ui->tableSettings->horizontalHeader()->setSectionResizeMode(VALUE, QHeaderView::Stretch);
    m_ui->tableSettings->setColumnWidth(ACTION,  SETTINGS_ACTION);

    m_ui->tableSettings->setItemDelegate(new BlinkDelegate(this));
    m_ui->tableSettings->setWordWrap(true);
    m_ui->tableSettings->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ui->tableSettings->setSelectionBehavior(QAbstractItemView::SelectItems);

    // ── Properties table ──────────────────────────────────────────────────────
    m_ui->tableProperties->setModel(m_propertiesModel);
    m_ui->tableProperties->horizontalHeader()->setStretchLastSection(false);
    m_ui->tableProperties->setColumnWidth(TableConfig::PropertiesColumns::LINE,     PROPERTIES_LINE);
    m_ui->tableProperties->setColumnWidth(TableConfig::PropertiesColumns::PROPERTY, PROPERTIES_PROPERTY);
    m_ui->tableProperties->horizontalHeader()->setSectionResizeMode(
        TableConfig::PropertiesColumns::VALUE, QHeaderView::Stretch);
    m_ui->tableProperties->setColumnWidth(TableConfig::PropertiesColumns::ACTION, PROPERTIES_ACTION);

    m_ui->tableProperties->setItemDelegate(new BlinkDelegate(this));
    m_ui->tableProperties->setWordWrap(true);
    m_ui->tableProperties->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ui->tableProperties->setSelectionBehavior(QAbstractItemView::SelectItems);
}

void ConfigurationController::setupSDKTab()
{
    using namespace TableConfig::PropertyDefColumns;
    using namespace TableConfig::ColumnWidths;

    m_ui->tablePropertyDefinitions->setModel(m_propertyDefinitionModel);
    m_ui->tablePropertyDefinitions->horizontalHeader()->setStretchLastSection(false);

    m_ui->tablePropertyDefinitions->setColumnWidth(NAME,          PROPDEF_NAME);
    m_ui->tablePropertyDefinitions->setColumnWidth(ID,            PROPDEF_ID);
    m_ui->tablePropertyDefinitions->setColumnWidth(SUPPORTED,     PROPDEF_SUPPORTED);
    m_ui->tablePropertyDefinitions->setColumnWidth(VALUE,         PROPDEF_DEFAULT);
    m_ui->tablePropertyDefinitions->setColumnWidth(NEED_REBOOT,   PROPDEF_NEED_REBOOT);
    m_ui->tablePropertyDefinitions->setColumnWidth(TYPE,          PROPDEF_TYPE);
    m_ui->tablePropertyDefinitions->setColumnWidth(READ_ONLY,     PROPDEF_READ_ONLY);
    m_ui->tablePropertyDefinitions->setColumnWidth(SET_BUTTON,    PROPDEF_SET_BUTTON);
    m_ui->tablePropertyDefinitions->setColumnWidth(GET_BUTTON,    PROPDEF_GET_BUTTON);
    m_ui->tablePropertyDefinitions->setColumnWidth(REMOVE_BUTTON, PROPDEF_REMOVE_BUTTON);

    m_ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(NAME,  QHeaderView::Stretch);
    m_ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(VALUE, QHeaderView::Stretch);
    m_ui->tablePropertyDefinitions->horizontalHeader()->setSectionResizeMode(ID,    QHeaderView::Fixed);
    m_ui->tablePropertyDefinitions->setWordWrap(true);
    m_ui->tablePropertyDefinitions->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ui->tablePropertyDefinitions->setSelectionBehavior(QAbstractItemView::SelectItems);

    applyPropDefColumnVisibility({
        true, true, false, false, true, true, false, true, true, true, true,
    });

    m_ui->tablePropertyDefinitions->setItemDelegate(new BlinkDelegate(this));

    connect(m_propertyDefinitionModel, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex &topLeft, const QModelIndex &, const QList<int> &) {
        if (m_propertyDefinitionModel->isUpdatingFromSocket())
            return;
        if (topLeft.column() != TableConfig::PropertyDefColumns::VALUE)
            return;
        const QString newValue = m_propertyDefinitionModel->data(topLeft, Qt::DisplayRole).toString();
        if (!newValue.isEmpty())
            onSetPropertyDefinitionClicked(topLeft.row());
    });

    QCompleter *completer = new QCompleter(new QStringListModel(this), this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    WidgetStyling::styleCompleterPopup(completer->popup());
    m_ui->txtPropertySearch->setCompleter(completer);

    connect(completer->popup(), &QAbstractItemView::clicked,
            this, [this, completer](const QModelIndex &index) {
        const QString text = completer->completionModel()->data(index).toString();
        if (!text.isEmpty())
            m_ui->txtPropertySearch->setText(text);
        onAddPropertyDefinition();
    });

    connect(m_ui->txtPropertySearch, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.isEmpty()) {
            QCompleter *c = m_ui->txtPropertySearch->completer();
            if (c && c->model() && c->model()->rowCount() > 0) {
                c->setCompletionPrefix("");
                c->complete();
            }
        }
    });

    m_ui->txtPropertySearch->installEventFilter(m_mainWindow);
}

void ConfigurationController::applyPropDefColumnVisibility(const QVector<bool> &vis)
{
    using namespace TableConfig::PropertyDefColumns;
    for (int c = 0; c < vis.size() && c < TOTAL_COLUMNS; ++c)
        m_ui->tablePropertyDefinitions->setColumnHidden(c, !vis[c]);
}

void ConfigurationController::updatePropertyNamesCompleter()
{
    QStringList names;
    names.reserve(m_availablePropertyDefinitions.size());
    for (const PropertyDefinition &p : m_availablePropertyDefinitions)
        names.append(p.name);

    QCompleter *completer = m_ui->txtPropertySearch->completer();
    if (completer)
        qobject_cast<QStringListModel *>(completer->model())->setStringList(names);
}

void ConfigurationController::clearAvailableCache()
{
    m_availablePropertyDefinitions.clear();
    updatePropertyNamesCompleter();
}

void ConfigurationController::wirePropertyRowButtons(int row)
{
    using namespace TableConfig::PropertyDefColumns;

    auto makeCenteredBtn = [&](const QString &iconPath, const QString &tooltip)
        -> std::pair<QWidget*, QPushButton*>
    {
        auto *btn = UiComponents::Button::icon(QIcon(iconPath), tooltip,
                                               nullptr,
                                               UiComponents::ButtonSize::Small);
        auto *cell = new QWidget();
        cell->setStyleSheet("background: transparent;");
        auto *lay = new QHBoxLayout(cell);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setAlignment(Qt::AlignCenter);
        lay->addWidget(btn);
        return {cell, btn};
    };

    auto [cellSet, btnSet] = makeCenteredBtn(":/icons/download.svg", "Set property value");
    if (m_monitoringPropertyDefs) btnSet->setEnabled(false);
    m_ui->tablePropertyDefinitions->setIndexWidget(
        m_propertyDefinitionModel->index(row, SET_BUTTON), cellSet);
    connect(btnSet, &QPushButton::clicked, this, [this, row]() { onSetPropertyDefinitionClicked(row); });

    auto [cellGet, btnGet] = makeCenteredBtn(":/icons/refresh.svg", "Get property value");
    m_ui->tablePropertyDefinitions->setIndexWidget(
        m_propertyDefinitionModel->index(row, GET_BUTTON), cellGet);
    connect(btnGet, &QPushButton::clicked, this, [this, row]() { onGetPropertyDefinitionClicked(row); });

    auto [cellRemove, btnRemove] = makeCenteredBtn(":/icons/edit-delete.svg", "Remove property from list");
    m_ui->tablePropertyDefinitions->setIndexWidget(
        m_propertyDefinitionModel->index(row, REMOVE_BUTTON), cellRemove);
    connect(btnRemove, &QPushButton::clicked, this, [this, row]() { onRemovePropertyDefinitionClicked(row); });
}

void ConfigurationController::recreatePropertyDefinitionButtons()
{
    if (!m_propertyDefinitionModel) return;
    for (int row = 0; row < m_propertyDefinitionModel->rowCount(); ++row)
        wirePropertyRowButtons(row);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Monitor toggles (Settings / Properties / Property Defs)
// ─────────────────────────────────────────────────────────────────────────────
//
// Each "Monitor" button starts a 500 ms QTimer that re-issues the existing
// fetch path. The fetch handlers in this controller detect the monitoring
// flag and take a *lightweight* path: value-only model updates with no
// per-row button rebuild and no filter reset, so the table doesn't flicker
// and the user's filter stays intact (only currently-visible rows have
// their values mutated thanks to SettingsModel::updateSettings(..., false)).
//
// While monitoring is active, every per-row "save to device" button on the
// monitored table is disabled — round-tripping a write through 500 ms reads
// would race against the user's edit.

void ConfigurationController::setupMonitorButtons()
{
    using namespace UiComponents;

    auto makeIntervalCombo = [this]() {
        auto *c = new QComboBox(m_mainWindow);
        c->setToolTip(tr("Monitor tick interval"));
        c->addItem(tr("250 ms"),  250);
        c->addItem(tr("500 ms"),  500);
        c->addItem(tr("1000 ms"), 1000);
        c->addItem(tr("2000 ms"), 2000);
        c->setCurrentIndex(1); // default 500 ms
        return c;
    };

    // Style helper: every monitor button is a square icon-only toggle.
    // Idle = subtle dark grey background; Checked = vivid red to signal
    // "live polling now". No text, ever — see tooltip for explanation.
    static const char *kMonitorBtnQss =
        "QPushButton {"
        "  background-color: #3a3a3a; border: 1px solid #4a4a4a;"
        "  border-radius: 6px; padding: 0;"
        "}"
        "QPushButton:hover    { background-color: #454545; border-color: #5a5a5f; }"
        "QPushButton:checked  { background-color: #c93a3a; border-color: #c93a3a; }"
        "QPushButton:checked:hover { background-color: #d65454; border-color: #d65454; }";

    auto makeMonitorButton = [this](QHBoxLayout *row, QComboBox *combo) -> QPushButton * {
        if (!row) return nullptr;
        auto *btn = new QPushButton(m_mainWindow);
        btn->setIcon(QIcon(QStringLiteral(":/icons/activity.svg")));
        btn->setIconSize(QSize(18, 18));
        btn->setMinimumSize(30, 30);
        btn->setFixedHeight(30);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString::fromLatin1(kMonitorBtnQss));
        btn->setToolTip(tr("Live monitor: re-fetch this table on the selected interval.\n"
                           "While active, per-row write buttons are disabled."));
        // Insert just before the trailing refresh button so the order reads:
        //   [ Label ][ stretch ][ Monitor ][ interval ][ Refresh ]
        const int refreshIdx = row->count() - 1;
        row->insertWidget(refreshIdx,     btn);
        row->insertWidget(refreshIdx + 1, combo);
        return btn;
    };

    m_cmbIntervalSettings   = makeIntervalCombo();
    m_cmbIntervalProperties = makeIntervalCombo();
    m_btnMonitorSettings    = makeMonitorButton(m_ui->horizontalLayout_settingsHeader,   m_cmbIntervalSettings);
    m_btnMonitorProperties  = makeMonitorButton(m_ui->horizontalLayout_propertiesHeader, m_cmbIntervalProperties);

    // Property Defs: there's no dedicated header layout — the refresh button
    // (`btnFetchPropertyDefs`) lives in `horizontalLayout_propertySearch`
    // alongside the search field. Insert Monitor + combo just before it.
    if (auto *psRow = m_ui->horizontalLayout_propertySearch) {
        m_btnMonitorPropertyDefs = new QPushButton(m_mainWindow);
        m_btnMonitorPropertyDefs->setIcon(QIcon(QStringLiteral(":/icons/activity.svg")));
        m_btnMonitorPropertyDefs->setIconSize(QSize(18, 18));
        m_btnMonitorPropertyDefs->setMinimumSize(30, 30);
        m_btnMonitorPropertyDefs->setFixedHeight(30);
        m_btnMonitorPropertyDefs->setCheckable(true);
        m_btnMonitorPropertyDefs->setCursor(Qt::PointingHandCursor);
        m_btnMonitorPropertyDefs->setStyleSheet(QString::fromLatin1(kMonitorBtnQss));
        m_btnMonitorPropertyDefs->setToolTip(tr("Live monitor: re-fetch property definitions on the selected interval.\n"
                                                "While active, per-row Set buttons are disabled."));
        m_cmbIntervalPropertyDefs = makeIntervalCombo();
        const int idx = psRow->indexOf(m_ui->btnFetchPropertyDefs);
        if (idx >= 0) {
            psRow->insertWidget(idx,     m_btnMonitorPropertyDefs);
            psRow->insertWidget(idx + 1, m_cmbIntervalPropertyDefs);
        } else {
            psRow->addWidget(m_btnMonitorPropertyDefs);
            psRow->addWidget(m_cmbIntervalPropertyDefs);
        }
    }

    auto makeTimer = [this]() {
        auto *t = new QTimer(this);
        t->setInterval(500);
        return t;
    };
    m_monitorSettingsTimer     = makeTimer();
    m_monitorPropertiesTimer   = makeTimer();
    m_monitorPropertyDefsTimer = makeTimer();

    if (m_btnMonitorSettings)
        connect(m_btnMonitorSettings, &QPushButton::toggled, this, &ConfigurationController::onMonitorSettingsToggled);
    if (m_btnMonitorProperties)
        connect(m_btnMonitorProperties, &QPushButton::toggled, this, &ConfigurationController::onMonitorPropertiesToggled);
    if (m_btnMonitorPropertyDefs)
        connect(m_btnMonitorPropertyDefs, &QPushButton::toggled, this, &ConfigurationController::onMonitorPropertyDefsToggled);

    auto wireInterval = [this](QComboBox *combo, QTimer *timer, bool &flag) {
        if (!combo || !timer) return;
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [combo, timer, &flag](int) {
            const int ms = combo->currentData().toInt();
            timer->setInterval(ms);
            if (flag) { timer->stop(); timer->start(); }
        });
    };
    wireInterval(m_cmbIntervalSettings,     m_monitorSettingsTimer,     m_monitoringSettings);
    wireInterval(m_cmbIntervalProperties,   m_monitorPropertiesTimer,   m_monitoringProperties);
    wireInterval(m_cmbIntervalPropertyDefs, m_monitorPropertyDefsTimer, m_monitoringPropertyDefs);

    connect(m_monitorSettingsTimer, &QTimer::timeout, this, [this]() {
        const QString id = m_deviceIdProvider();
        if (id.isEmpty() || m_settingsRefreshBusy) return;
        if (m_settingsModel->visibleSettings().isEmpty()) return;
        m_settingsRefreshBusy = true;
        AdbManager::instance().fetchSettings(id);
    });
    connect(m_monitorPropertiesTimer, &QTimer::timeout, this, [this]() {
        const QString id = m_deviceIdProvider();
        if (id.isEmpty() || m_propertiesRefreshBusy) return;
        if (m_propertiesModel->visibleProperties().isEmpty()) return;
        m_propertiesRefreshBusy = true;
        AdbManager::instance().fetchProperties(id);
    });
    connect(m_monitorPropertyDefsTimer, &QTimer::timeout, this, [this]() {
        const QString id = m_deviceIdProvider();
        if (id.isEmpty() || m_propertyDefsRefreshBusy) return;
        // The property-definition table is user-curated: only entries the
        // user has explicitly added are present. There is no separate filter.
        const QVector<PropertyDefinition> defs = m_propertyDefinitionModel->getPropertyDefinitions();
        if (defs.isEmpty()) return;
        m_propertyDefsRefreshBusy = true;
        AdbManager::instance().fetchPropertyDefinitions(id);
    });
}

void ConfigurationController::onMonitorSettingsToggled(bool on)
{
    m_monitoringSettings = on;

    setSettingsRowActionsEnabled(!on);
    if (on) {
        const int ms = m_cmbIntervalSettings ? m_cmbIntervalSettings->currentData().toInt() : 500;
        m_monitorSettingsTimer->setInterval(ms);
        if (m_btnMonitorSettings) m_btnMonitorSettings->setText(QStringLiteral(" %1ms").arg(ms));
        m_monitorSettingsTimer->start();
        m_ui->statusbar->showMessage(tr("Monitoring settings every %1 ms").arg(ms), 2000);
    } else {
        m_monitorSettingsTimer->stop();
        if (m_btnMonitorSettings) m_btnMonitorSettings->setText(QString());
        m_ui->statusbar->showMessage(tr("Settings monitor stopped"), 2000);
    }
    emit monitorStateChanged((m_monitoringSettings?1:0) + (m_monitoringProperties?1:0) + (m_monitoringPropertyDefs?1:0));
    emit monitorTablesChanged(m_monitoringSettings, m_monitoringProperties, m_monitoringPropertyDefs);
}

void ConfigurationController::onMonitorPropertiesToggled(bool on)
{
    m_monitoringProperties = on;

    setPropertiesRowActionsEnabled(!on);
    if (on) {
        const int ms = m_cmbIntervalProperties ? m_cmbIntervalProperties->currentData().toInt() : 500;
        m_monitorPropertiesTimer->setInterval(ms);
        if (m_btnMonitorProperties) m_btnMonitorProperties->setText(QStringLiteral(" %1ms").arg(ms));
        m_monitorPropertiesTimer->start();
        m_ui->statusbar->showMessage(tr("Monitoring properties every %1 ms").arg(ms), 2000);
    } else {
        m_monitorPropertiesTimer->stop();
        if (m_btnMonitorProperties) m_btnMonitorProperties->setText(QString());
        m_ui->statusbar->showMessage(tr("Properties monitor stopped"), 2000);
    }
    emit monitorStateChanged((m_monitoringSettings?1:0) + (m_monitoringProperties?1:0) + (m_monitoringPropertyDefs?1:0));
    emit monitorTablesChanged(m_monitoringSettings, m_monitoringProperties, m_monitoringPropertyDefs);
}

void ConfigurationController::onMonitorPropertyDefsToggled(bool on)
{
    m_monitoringPropertyDefs = on;

    setPropertyDefSetButtonsEnabled(!on);
    if (on) {
        const int ms = m_cmbIntervalPropertyDefs ? m_cmbIntervalPropertyDefs->currentData().toInt() : 500;
        m_monitorPropertyDefsTimer->setInterval(ms);
        if (m_btnMonitorPropertyDefs) m_btnMonitorPropertyDefs->setText(QStringLiteral(" %1ms").arg(ms));
        m_monitorPropertyDefsTimer->start();
        m_ui->statusbar->showMessage(tr("Monitoring property definitions every %1 ms").arg(ms), 2000);
    } else {
        m_monitorPropertyDefsTimer->stop();
        if (m_btnMonitorPropertyDefs) m_btnMonitorPropertyDefs->setText(QString());
        m_ui->statusbar->showMessage(tr("Property-definition monitor stopped"), 2000);
    }
    emit monitorStateChanged((m_monitoringSettings?1:0) + (m_monitoringProperties?1:0) + (m_monitoringPropertyDefs?1:0));
    emit monitorTablesChanged(m_monitoringSettings, m_monitoringProperties, m_monitoringPropertyDefs);
}

void ConfigurationController::setSettingsRowActionsEnabled(bool enabled)
{
    using namespace TableConfig::SettingsColumns;
    const int rows = m_settingsModel->rowCount();
    for (int i = 0; i < rows; ++i) {
        QWidget *w = m_ui->tableSettings->indexWidget(m_settingsModel->index(i, ACTION));
        if (auto *btn = qobject_cast<QPushButton *>(w))
            btn->setEnabled(enabled);
    }
}

void ConfigurationController::setPropertiesRowActionsEnabled(bool enabled)
{
    using namespace TableConfig::PropertiesColumns;
    const int rows = m_propertiesModel->rowCount();
    for (int i = 0; i < rows; ++i) {
        QWidget *w = m_ui->tableProperties->indexWidget(m_propertiesModel->index(i, ACTION));
        if (auto *btn = qobject_cast<QPushButton *>(w))
            btn->setEnabled(enabled);
    }
}

void ConfigurationController::setPropertyDefSetButtonsEnabled(bool enabled)
{
    using namespace TableConfig::PropertyDefColumns;
    if (!m_propertyDefinitionModel) return;
    const int rows = m_propertyDefinitionModel->rowCount();
    for (int i = 0; i < rows; ++i) {
        // The cell holds a centring wrapper QWidget whose only child is the
        // Set button (see wirePropertyRowButtons).
        QWidget *cell = m_ui->tablePropertyDefinitions->indexWidget(
            m_propertyDefinitionModel->index(i, SET_BUTTON));
        if (!cell) continue;
        for (QPushButton *b : cell->findChildren<QPushButton *>())
            b->setEnabled(enabled);
    }
}

void ConfigurationController::stopAllMonitors()
{
    // Toggling each button off funnels through the matching slot which
    // stops its timer, restores per-row write buttons, and resets the label.
    for (QPushButton *b : { m_btnMonitorSettings,
                            m_btnMonitorProperties,
                            m_btnMonitorPropertyDefs }) {
        if (b && b->isChecked()) b->setChecked(false);
    }
}
