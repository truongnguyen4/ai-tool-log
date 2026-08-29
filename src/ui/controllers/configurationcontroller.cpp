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

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QPushButton>
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
    buildMonitorControls(m_propertyDefsMonitor,
                         m_ui->horizontalLayout_sdkToolbar,
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
        // A reload while a write runs would only show the values before it.
        if (m_deviceIdProvider().isEmpty() || m_propertyDefsMonitor.busy
            || m_propertyFetchInFlight || m_propertyWriteInFlight)
            return;
        m_propertyDefsMonitor.busy = true;
        refreshPropertyDefinitions();
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
