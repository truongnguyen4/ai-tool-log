// UiManager: initial widget construction / model wiring.
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "colorscheme.h"
#include "configurationcontroller.h"
#include "cradlecontroller.h"
#include "devicesmanager.h"
#include "dumpsyscontroller.h"
#include "highlightdelegate.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "settingsmodel.h"
#include "tableconfig.h"
#include "tablestyler.h"
#include "themesheets.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QListView>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringListModel>
#include <QTableView>
#include <QTabBar>
#include <QTabWidget>
#include <QScrollBar>
#include <QtConcurrent>

// Defines: setupLogTable, setupConfigurationTables,
// setupDumpsys, setupCradleTab.


// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Setup — initialise visual components
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::setupMainNavigationTabs()
{
    auto *tabs = m_ui->tabWidget;
    if (!tabs) return;

    tabs->setTabPosition(QTabWidget::West);
    tabs->setIconSize(QSize(22, 22));

    if (QTabBar *bar = tabs->tabBar()) {
        bar->setExpanding(false);
        bar->setUsesScrollButtons(false);
        bar->setDrawBase(false);
        bar->setFocusPolicy(Qt::TabFocus);
    }

    struct MainTabSpec {
        QWidget *page;
        QString iconPath;
        QString title;
    };

    const QVector<MainTabSpec> specs = {
        {m_ui->tabAdbLogcat,     QStringLiteral(":/icons/tab-logcat.svg"),  tr("Logcat")},
        {m_ui->tabConfiguration, QStringLiteral(":/icons/tab-android.svg"), tr("Android")},
        {m_ui->tabSDK,           QStringLiteral(":/icons/tab-sdk.svg"),     tr("SDK")},
        {m_ui->tabDevices,       QStringLiteral(":/icons/tab-devices.svg"), tr("Devices")},
    };

    for (const MainTabSpec &spec : specs) {
        const int index = tabs->indexOf(spec.page);
        if (index < 0)
            continue;

        tabs->setTabIcon(index, QIcon(spec.iconPath));
        tabs->setTabText(index, QString());
        tabs->setTabToolTip(index, spec.title);
        tabs->setTabWhatsThis(index, spec.title);
        if (QTabBar *bar = tabs->tabBar())
            bar->setTabData(index, spec.title);
        if (spec.page)
            spec.page->setAccessibleName(spec.title);
    }
}

void UiManager::applyLogColumnWidths(QTableView *view)
{
    using namespace TableConfig::LogColumns;
    using namespace TableConfig::ColumnWidths;

    view->horizontalHeader()->setStretchLastSection(false);
    view->setColumnWidth(DATE,    LOG_DATE);
    view->setColumnWidth(TIME,    LOG_TIME);
    view->setColumnWidth(PID,     LOG_PID);
    view->setColumnWidth(TID,     LOG_TID);
    view->setColumnWidth(PACKAGE, LOG_PACKAGE);
    view->setColumnWidth(LEVEL,   LOG_LEVEL);
    view->horizontalHeader()->setSectionResizeMode(MESSAGE, QHeaderView::Stretch);
}

void UiManager::installLogHighlightDelegates(QTableView *view, bool paneB)
{
    using namespace TableConfig::LogColumns;

    HighlightDelegate *pid     = paneB ? m_pidHighlightDelegateB     : m_pidHighlightDelegate;
    HighlightDelegate *package = paneB ? m_packageHighlightDelegateB : m_packageHighlightDelegate;
    HighlightDelegate *tag     = paneB ? m_tagHighlightDelegateB     : m_tagHighlightDelegate;
    HighlightDelegate *message = paneB ? m_messageHighlightDelegateB : m_messageHighlightDelegate;

    message->setWordWrap(true);
    view->setItemDelegateForColumn(PID,     pid);
    view->setItemDelegateForColumn(PACKAGE, package);
    view->setItemDelegateForColumn(TAG,     tag);
    view->setItemDelegateForColumn(MESSAGE, message);

    // A keyword-less HighlightDelegate at view level so DATE, TIME, TID and
    // LEVEL also honour Qt::BackgroundRole for marked rows. (Per-column
    // delegates take priority, hence the two-level setup.)
    view->setItemDelegate(new HighlightDelegate(this));
}

void UiManager::wireRowResizeTriggers(QTableView *view, std::function<bool()> hasRows)
{
    using namespace TableConfig::LogColumns;

    // Word-wrapped rows have to be re-measured when their column gets wider or
    // when different rows scroll into view. Both go through the debounce timer
    // so a drag or a flick costs one measurement pass, not one per event.
    connect(view->horizontalHeader(), &QHeaderView::sectionResized, this,
            [this](int section, int, int) {
                if (section == TAG || section == MESSAGE)
                    m_rowResizeTimer->start();
            });
    connect(view->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this, hasRows = std::move(hasRows)](int) {
                if (hasRows())
                    m_rowResizeTimer->start();
            });
}

void UiManager::setupLogTable()
{
    using namespace TableConfig::LogColumns;

    // ── Models ────────────────────────────────────────────────────────────────
    m_ui->tableLog->setModel(m_logModel);
    m_logModel->setMarkedRows(&m_markedRows);
    m_ui->tableMarkLog->setModel(m_markLogModel);

    // ── Columns ───────────────────────────────────────────────────────────────
    applyLogColumnWidths(m_ui->tableLog);
    applyLogColumnWidths(m_ui->tableMarkLog);
    m_ui->tableMarkLog->horizontalHeader()->setSectionResizeMode(DELTA,
                                                                 QHeaderView::ResizeToContents);

    // Hide low-value columns by default and sync the dependent filter groups.
    applyColumnVisibility({false, false, true, true, false, true, true, true});

    // ── Row-resize debounce timer (created before anything can trigger it) ────
    m_rowResizeTimer = new QTimer(this);
    m_rowResizeTimer->setSingleShot(true);
    m_rowResizeTimer->setInterval(UiTiming::kRowResizeDebounceMs);
    connect(m_rowResizeTimer, &QTimer::timeout, this, &UiManager::resizeVisibleRows);

    // ── Highlight delegates (pane A) ──────────────────────────────────────────
    m_pidHighlightDelegate     = new HighlightDelegate(this);
    m_packageHighlightDelegate = new HighlightDelegate(this);
    m_tagHighlightDelegate     = new HighlightDelegate(this);
    m_messageHighlightDelegate = new HighlightDelegate(this);
    installLogHighlightDelegates(m_ui->tableLog, /*paneB=*/false);
    wireRowResizeTriggers(m_ui->tableLog,
                          [this]() { return m_logModel->rowCount() > 0; });

    TableStyler::applyLogTableStyle({m_ui->tableLog, m_ui->tableMarkLog});
}

void UiManager::setupConfigurationTables()
{
    if (!m_configurationController) {
        m_configurationController = new ConfigurationController(
            m_ui, m_mainWindow,
            m_settingsModel, m_propertiesModel, m_propertyDefinitionModel,
            [this]() { return m_currentDeviceId; },
            this);
    }
    m_configurationController->setupTables();
    m_configurationController->setupMonitorButtons();

    // ── Config splitters ──────────────────────────────────────────────────────
    // Proportional sizing: scales with window width (used as initial fallback;
    // restoreLayoutPreferences() may overwrite these from QSettings).
    m_ui->splitterConfig->setStretchFactor(0, 5);
    m_ui->splitterConfig->setStretchFactor(1, 7);
    m_ui->splitterConfigTables->setStretchFactor(0, 1);
    m_ui->splitterConfigTables->setStretchFactor(1, 1);
    const int cw = m_mainWindow->width() > 0 ? m_mainWindow->width() : 1200;
    m_ui->splitterConfig->setSizes(QList<int>() << cw * 5 / 12 << cw * 7 / 12);
    m_ui->splitterConfigTables->setSizes(QList<int>() << cw / 2 << cw / 2);

    convertSdkTabsToSidebar();
}

// ─────────────────────────────────────────────────────────────────────────────
// Convert tabWidgetSDK (a horizontal QTabWidget) into a vertical sidebar:
// QListWidget on the left + QStackedWidget on the right. Sidebar entries
// render with horizontal text — Qt does NOT rotate QListWidget items the way
// it rotates a West-positioned QTabBar.
// ─────────────────────────────────────────────────────────────────────────────
void UiManager::convertSdkTabsToSidebar()
{
    auto *tabs = m_ui->tabWidgetSDK;
    if (!tabs) return;

    // Capture pages + titles before tearing down the tab widget.
    struct Page { QString title; QWidget *widget; };
    QVector<Page> pages;
    pages.reserve(tabs->count());
    for (int i = 0; i < tabs->count(); ++i)
        pages.push_back({ tabs->tabText(i), tabs->widget(i) });

    // Build the new container with sidebar + stack.
    auto *container = new QWidget(tabs->parentWidget());
    container->setObjectName(QStringLiteral("sdkSidebarContainer"));
    auto *hl = new QHBoxLayout(container);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    auto *sidebar = new QListWidget(container);
    sidebar->setObjectName(QStringLiteral("sdkSidebar"));
    sidebar->setFrameShape(QFrame::NoFrame);
    sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sidebar->setSelectionMode(QAbstractItemView::SingleSelection);
    sidebar->setUniformItemSizes(true);
    sidebar->setFixedWidth(180);

    auto *stack = new QStackedWidget(container);
    stack->setObjectName(QStringLiteral("sdkStack"));

    // Re-parent each page widget into the stack and add a sidebar entry.
    for (const auto &p : pages) {
        if (!p.widget) continue;
        p.widget->setParent(stack);
        stack->addWidget(p.widget);
        auto *item = new QListWidgetItem(p.title, sidebar);
        item->setSizeHint(QSize(0, 44));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    hl->addWidget(sidebar);
    hl->addWidget(stack, 1);

    // Drive the stack from the sidebar selection.
    QObject::connect(sidebar, &QListWidget::currentRowChanged,
                     stack,   &QStackedWidget::setCurrentIndex);
    sidebar->setCurrentRow(0);

    // Swap the QTabWidget out of its parent layout for the new container.
    if (auto *parentLayout = tabs->parentWidget() ? tabs->parentWidget()->layout() : nullptr) {
        const int idx = parentLayout->indexOf(tabs);
        if (idx >= 0) {
            QLayoutItem *taken = parentLayout->takeAt(idx);
            delete taken;
            if (auto *box = qobject_cast<QBoxLayout *>(parentLayout))
                box->insertWidget(idx, container, 1);
            else
                parentLayout->addWidget(container);
        }
    }
    tabs->hide();
    tabs->deleteLater();
}

void UiManager::setupTabAutoFetch()
{
    // Auto-fetch relevant data when switching to Configuration or SDK tabs.
    connect(m_ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        QWidget *tab = m_ui->tabWidget->widget(index);
        if (tab == m_ui->tabConfiguration && !m_currentDeviceId.isEmpty()) {
            AdbManager::instance().fetchDumpsysList(m_currentDeviceId);
            AdbManager::instance().fetchSettings(m_currentDeviceId);
            AdbManager::instance().fetchProperties(m_currentDeviceId);
        } else if (tab == m_ui->tabSDK && !m_currentDeviceId.isEmpty()) {
            AdbManager::instance().fetchPropertyDefinitions(m_currentDeviceId);
        }
    });
}
