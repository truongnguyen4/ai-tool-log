#include "configurationcontroller.h"
#include "ui_mainwindow.h"
#include "tableconfig.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "blinkdelegate.h"
#include "rowactiondelegate.h"
#include "tablestyler.h"
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

namespace {
/** Default monitor tick, and the option list offered in the interval combo. */
constexpr int kDefaultIntervalMs = 500;
constexpr int kIntervalOptionsMs[] = {250, 500, 1000, 2000};
constexpr int kDefaultIntervalIndex = 1;
constexpr QSize kMonitorButtonSize{30, 30};
constexpr QSize kMonitorIconSize{18, 18};
} // namespace

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
    m_settingsMonitor.name     = tr("settings");
    m_propertiesMonitor.name   = tr("properties");
    m_propertyDefsMonitor.name = tr("property definitions");
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Table setup
// ─────────────────────────────────────────────────────────────────────────────

RowActionDelegate *ConfigurationController::addRowAction(
    QTableView *view, int column, const QString &iconPath, const QString &tooltip,
    void (ConfigurationController::*slot)(int))
{
    auto *action = new RowActionDelegate(QIcon(iconPath), tooltip, this);
    action->installOn(view, column);
    connect(action, &RowActionDelegate::triggered, this, slot);
    return action;
}

void ConfigurationController::setupTables()
{
    using namespace TableConfig::ColumnWidths;

    // ── Settings table ────────────────────────────────────────────────────────
    {
        using namespace TableConfig::SettingsColumns;
        auto *table = m_ui->tableSettings;
        table->setModel(m_settingsModel);
        table->horizontalHeader()->setStretchLastSection(false);
        table->setColumnWidth(LINE,    SETTINGS_LINE);
        table->setColumnWidth(GROUP,   SETTINGS_GROUP);
        table->setColumnWidth(SETTING, SETTINGS_SETTING);
        table->horizontalHeader()->setSectionResizeMode(VALUE, QHeaderView::Stretch);
        table->setColumnWidth(ACTION,  SETTINGS_ACTION);

        table->setItemDelegate(new BlinkDelegate(this));
        m_settingsMonitor.rowActions = {
            addRowAction(table, ACTION, QStringLiteral(":/icons/download.svg"),
                         tr("Write this setting to the device"),
                         &ConfigurationController::onSaveSettingClicked)
        };
    }

    // ── Properties table ──────────────────────────────────────────────────────
    {
        using namespace TableConfig::PropertiesColumns;
        auto *table = m_ui->tableProperties;
        table->setModel(m_propertiesModel);
        table->horizontalHeader()->setStretchLastSection(false);
        table->setColumnWidth(LINE,     PROPERTIES_LINE);
        table->setColumnWidth(PROPERTY, PROPERTIES_PROPERTY);
        table->horizontalHeader()->setSectionResizeMode(VALUE, QHeaderView::Stretch);
        table->setColumnWidth(ACTION,   PROPERTIES_ACTION);

        table->setItemDelegate(new BlinkDelegate(this));
        m_propertiesMonitor.rowActions = {
            addRowAction(table, ACTION, QStringLiteral(":/icons/download.svg"),
                         tr("Write this property to the device"),
                         &ConfigurationController::onSavePropertyClicked)
        };
    }

    TableStyler::applyConfigTableStyle({m_ui->tableSettings, m_ui->tableProperties});
}

void ConfigurationController::setupSDKTab()
{
    using namespace TableConfig::PropertyDefColumns;
    using namespace TableConfig::ColumnWidths;

    auto *table = m_ui->tablePropertyDefinitions;
    table->setModel(m_propertyDefinitionModel);
    table->horizontalHeader()->setStretchLastSection(false);

    table->setColumnWidth(NAME,          PROPDEF_NAME);
    table->setColumnWidth(ID,            PROPDEF_ID);
    table->setColumnWidth(SUPPORTED,     PROPDEF_SUPPORTED);
    table->setColumnWidth(VALUE,         PROPDEF_DEFAULT);
    table->setColumnWidth(NEED_REBOOT,   PROPDEF_NEED_REBOOT);
    table->setColumnWidth(TYPE,          PROPDEF_TYPE);
    table->setColumnWidth(READ_ONLY,     PROPDEF_READ_ONLY);
    table->setColumnWidth(SET_BUTTON,    PROPDEF_SET_BUTTON);
    table->setColumnWidth(GET_BUTTON,    PROPDEF_GET_BUTTON);
    table->setColumnWidth(REMOVE_BUTTON, PROPDEF_REMOVE_BUTTON);

    table->horizontalHeader()->setSectionResizeMode(NAME,  QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(VALUE, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(ID,    QHeaderView::Fixed);

    applyPropDefColumnVisibility({
        true, true, false, false, true, true, false, true, true, true, true,
    });

    table->setItemDelegate(new BlinkDelegate(this));
    TableStyler::applyConfigTableStyle({table});

    // Row actions. Only "Set" writes to the device, so only it is suspended
    // while the monitor is polling.
    auto *setAction = addRowAction(table, SET_BUTTON, QStringLiteral(":/icons/download.svg"),
                                   tr("Write this property value to the device"),
                                   &ConfigurationController::onSetPropertyDefinitionClicked);
    addRowAction(table, GET_BUTTON, QStringLiteral(":/icons/refresh.svg"),
                 tr("Read this property value from the device"),
                 &ConfigurationController::onGetPropertyDefinitionClicked);
    addRowAction(table, REMOVE_BUTTON, QStringLiteral(":/icons/edit-delete.svg"),
                 tr("Remove this property from the list"),
                 &ConfigurationController::onRemovePropertyDefinitionClicked);
    m_propertyDefsMonitor.rowActions = {setAction};

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
        if (!text.isEmpty())
            return;
        QCompleter *c = m_ui->txtPropertySearch->completer();
        if (c && c->model() && c->model()->rowCount() > 0) {
            c->setCompletionPrefix(QString());
            c->complete();
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
    if (!completer)
        return;
    if (auto *model = qobject_cast<QStringListModel *>(completer->model()))
        model->setStringList(names);
}

void ConfigurationController::clearAvailableCache()
{
    m_availablePropertyDefinitions.clear();
    updatePropertyNamesCompleter();
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Live monitors (Settings / Properties / Property Defs)
// ─────────────────────────────────────────────────────────────────────────────
//
// Each "Monitor" toggle re-issues the pane's existing fetch on a tick. The
// fetch handlers detect the active flag and take a *lightweight* path:
// value-only model updates with no filter reset, so the table doesn't flicker
// and the user's filter stays intact.
//
// While a monitor is active every write action on that table is disabled —
// round-tripping a write through sub-second reads would race the user's edit.

void ConfigurationController::buildMonitorControls(MonitorPane &pane,
                                                   QHBoxLayout *row,
                                                   QWidget *anchor)
{
    if (!row)
        return;

    pane.button = new QPushButton(m_mainWindow);
    pane.button->setIcon(QIcon(QStringLiteral(":/icons/activity.svg")));
    pane.button->setIconSize(kMonitorIconSize);
    pane.button->setMinimumSize(kMonitorButtonSize);
    pane.button->setFixedHeight(kMonitorButtonSize.height());
    pane.button->setCheckable(true);
    pane.button->setCursor(Qt::PointingHandCursor);
    // Styled from the theme sheet via QPushButton[role="monitor"] — no inline QSS.
    pane.button->setProperty("role", QStringLiteral("monitor"));
    pane.button->setToolTip(tr("Live monitor: re-fetch %1 on the selected interval.\n"
                               "While active, per-row write actions are disabled.")
                                .arg(pane.name));
    pane.button->style()->unpolish(pane.button);
    pane.button->style()->polish(pane.button);

    pane.interval = new QComboBox(m_mainWindow);
    pane.interval->setToolTip(tr("Monitor tick interval"));
    for (int ms : kIntervalOptionsMs)
        pane.interval->addItem(tr("%1 ms").arg(ms), ms);
    pane.interval->setCurrentIndex(kDefaultIntervalIndex);

    // Insert just before the anchor widget so the order reads:
    //   [ Label ][ stretch ][ Monitor ][ interval ][ anchor ]
    const int idx = anchor ? row->indexOf(anchor) : row->count() - 1;
    if (idx >= 0) {
        row->insertWidget(idx,     pane.button);
        row->insertWidget(idx + 1, pane.interval);
    } else {
        row->addWidget(pane.button);
        row->addWidget(pane.interval);
    }

    pane.timer = new QTimer(this);
    pane.timer->setInterval(kDefaultIntervalMs);

    connect(pane.button, &QPushButton::toggled, this,
            [this, &pane](bool on) { setMonitorActive(pane, on); });
    connect(pane.interval, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [&pane](int) {
                pane.timer->setInterval(pane.interval->currentData().toInt());
                if (pane.active) {
                    pane.timer->stop();
                    pane.timer->start();
                }
            });
}

void ConfigurationController::setupMonitorButtons()
{
    buildMonitorControls(m_settingsMonitor,
                         m_ui->horizontalLayout_settingsHeader,
                         m_ui->btnRefreshSettings);
    buildMonitorControls(m_propertiesMonitor,
                         m_ui->horizontalLayout_propertiesHeader,
                         m_ui->btnRefreshProperties);
    // Property Defs has no dedicated header row — its refresh button lives in
    // the search row, so anchor the monitor controls to that instead.
    buildMonitorControls(m_propertyDefsMonitor,
                         m_ui->horizontalLayout_propertySearch,
                         m_ui->btnFetchPropertyDefs);

    connect(m_settingsMonitor.timer, &QTimer::timeout, this, [this]() {
        const QString id = m_deviceIdProvider();
        if (id.isEmpty() || m_settingsMonitor.busy) return;
        if (m_settingsModel->visibleSettings().isEmpty()) return;
        m_settingsMonitor.busy = true;
        AdbManager::instance().fetchSettings(id);
    });
    connect(m_propertiesMonitor.timer, &QTimer::timeout, this, [this]() {
        const QString id = m_deviceIdProvider();
        if (id.isEmpty() || m_propertiesMonitor.busy) return;
        if (m_propertiesModel->visibleProperties().isEmpty()) return;
        m_propertiesMonitor.busy = true;
        AdbManager::instance().fetchProperties(id);
    });
    connect(m_propertyDefsMonitor.timer, &QTimer::timeout, this, [this]() {
        const QString id = m_deviceIdProvider();
        if (id.isEmpty() || m_propertyDefsMonitor.busy) return;
        // The property-definition table is user-curated: only entries the
        // user explicitly added are present, so there is no separate filter.
        if (m_propertyDefinitionModel->getPropertyDefinitions().isEmpty()) return;
        m_propertyDefsMonitor.busy = true;
        AdbManager::instance().fetchPropertyDefinitions(id);
    });
}

void ConfigurationController::setMonitorActive(MonitorPane &pane, bool on)
{
    pane.active = on;

    for (RowActionDelegate *action : std::as_const(pane.rowActions))
        action->setEnabled(!on);
    if (pane.button)
        pane.button->setText(QString());

    if (on) {
        const int ms = pane.interval ? pane.interval->currentData().toInt()
                                     : kDefaultIntervalMs;
        pane.timer->setInterval(ms);
        pane.timer->start();
        if (pane.button)
            pane.button->setText(QStringLiteral(" %1ms").arg(ms));
        m_ui->statusbar->showMessage(
            tr("Monitoring %1 every %2 ms").arg(pane.name).arg(ms), 2000);
    } else {
        pane.timer->stop();
        pane.busy = false;
        m_ui->statusbar->showMessage(tr("Stopped monitoring %1").arg(pane.name), 2000);
    }

    // Repaint the action column so the new enabled state is visible at once.
    for (QTableView *view : {m_ui->tableSettings, m_ui->tableProperties,
                             m_ui->tablePropertyDefinitions}) {
        if (view)
            view->viewport()->update();
    }

    publishMonitorState();
}

void ConfigurationController::publishMonitorState()
{
    const bool s = m_settingsMonitor.active;
    const bool p = m_propertiesMonitor.active;
    const bool d = m_propertyDefsMonitor.active;
    emit monitorStateChanged(int(s) + int(p) + int(d));
    emit monitorTablesChanged(s, p, d);
}

void ConfigurationController::stopAllMonitors()
{
    // Toggling each button off funnels through setMonitorActive(), which stops
    // the tick, restores the row actions, and resets the label.
    for (MonitorPane *pane : {&m_settingsMonitor, &m_propertiesMonitor,
                              &m_propertyDefsMonitor}) {
        if (pane->button && pane->button->isChecked())
            pane->button->setChecked(false);
    }
}
