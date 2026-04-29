#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QString>
#include <QList>
#include <QFutureWatcher>
#include <QMap>
#include <QPoint>

#include "adbmanager.h"
#include "devicesmanager.h"
#include "ilogconverter.h"
#include "filemanager.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "historymanager.h"
#include "highlightdelegate.h"
#include "ilogfilter.h"
#include "logfilter.h"
#include "logfiltercontroller.h"
#include "logsplitcontroller.h"
#include <QStringList>
#include <QPushButton>

class QTableView;
class QWheelEvent;
class MainWindow;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ---------------------------------------------------------------------------
// Result bundle produced by the background file-loading thread.
// IDs are assigned on the worker thread so the main thread only needs
// to std::move the data into place (O(1) transfers).
// ---------------------------------------------------------------------------
struct FileLoadResult {
    QVector<LogEntry>    entries;
    QHash<quint64, int>  allLogsIndex;
    quint64              nextLogId   = 0;
    LogConverterPtr      converter;
    QString              errorMsg;
    QString              filePath;
    int                  parsedCount = 0;
    int                  lineCount   = 0;
};

// ---------------------------------------------------------------------------
// UiManager — adapter between MainWindow and all application logic.
//
// Responsibilities:
//   • Own all models, data containers, delegates, and timers
//   • Set up the UI (table columns, delegates, splitters, tooltips, …)
//   • Wire every signal/slot connection between widgets and logic
//   • Implement every user-action handler (filter, device, file I/O, …)
//
// MainWindow is reduced to a thin shell that:
//   • Calls initialize() once after ui->setupUi()
//   • Delegates eventFilter() to handleEvent()
// ---------------------------------------------------------------------------
class UiManager : public QObject
{
    Q_OBJECT

    friend class DevicesTabController;

public:
    // Device display info derived from a real AdbDevice + group assignment.
    struct DeviceInfo {
        QString serial;        // adb device ID (unique key)
        QString name;          // display name (model or serial)
        QString group;         // group the device is assigned to
        bool    online = false;
    };

public:
    explicit UiManager(Ui::MainWindow *ui, MainWindow *mainWindow);

    // Called once by MainWindow::MainWindow() after ui->setupUi().
    void initialize();

    // Delegated from MainWindow::eventFilter().
    bool handleEvent(QObject *obj, QEvent *event);

    // Called from MainWindow::closeEvent() to flush filter history to QSettings.
    void persistFilterHistory();

private slots:
    void applyCurrentTheme();

private:
    // =========================================================================
    // SECTION: Setup — initialise visual components
    // =========================================================================
    void setupMainNavigationTabs();
    void setupLogTable();
    void setupConfigurationTables();
    void convertSdkTabsToSidebar();
    void setupTabAutoFetch();
    void setupTooltips();
    void setupToolbarDividers();
    void setupStatusBarIndicators();
    void setupSplittersAndMisc();

    // U6: persist column widths + splitter sizes between sessions.
    void saveLayoutPreferences();
    void restoreLayoutPreferences();

    // =========================================================================
    // SECTION: Signal connections — group by feature area
    // =========================================================================
    void connectAdbManagerSignals();
    void connectFilterSignals();
    void connectButtonSignals();
    void connectTableSignals();

    // =========================================================================
    // SECTION: Filter & Highlight
    // =========================================================================
    void onFilterChanged();
    void onHighlightChanged();
    void onHighlightNextClicked();
    void onHighlightPrevClicked();
    void updateFilterHighlighting();
    void onSettingsFilterChanged();
    void onPropertiesFilterChanged();
    FilterCriteria buildFilterCriteria() const;
    void applyFilters();
    void updateFilterCount();
    bool passesFilter(const LogEntry &entry);
    void setupFilterCompleters();

    // =========================================================================
    // SECTION: Device / Logcat
    // =========================================================================
    void onDeviceChanged(int index);
    void onDevicesChanged(const QList<AdbDevice> &devices);
    void onLogcatLineReceived(const QString &line);
    void onStartClicked();
    void onClearClicked();
    void flushPendingLines();

    // Status-bar feedback helpers — single source of truth for transient
    // toolbar messages. Kept side-by-side with the logcat slots so all
    // message-emitting code reads the same way (UiManager::flashStatus(...)).
    void flashStatus(const QString &message);

    // Toolbar visuals for the logcat / kernel toggle pair. Encapsulates the
    // active-button stylesheet + cross-button enable/disable used by both
    // click slots and AdbManager state callbacks (logcatStarted/Stopped,
    // dmesgStopped/Failed).
    void setLogcatRunningVisuals(bool running);
    void setKernelRunningVisuals(bool running);

    // =========================================================================
    // SECTION: Kernel (dmesg)
    // =========================================================================
    void onKernelClicked();
    void parseDmesgLine(const QString &line);

    // =========================================================================
    // SECTION: File I/O
    // =========================================================================
    void onLoadFileClicked();
    void onOpenFileClicked();
    void onSaveFileClicked();
    void loadLogsFromFile(const QString &filePath);
    void onFileLoadFinished();

    // =========================================================================
    // SECTION: Table Interaction
    // =========================================================================
    void onTableContextMenu(const QPoint &pos);
    void addToFilter(const QString &filterType, const QString &value, FilterOperator op);
    void showCellContentDialog(const QString &content, QWidget *parent = nullptr);
    void onLogTableDoubleClicked(const QModelIndex &index);
    void onLogTableClicked(const QModelIndex &index);
    void onMarkLogTableClicked(const QModelIndex &index);
    void onMarkLogContextMenu(const QPoint &pos);
    void onClearAllMarkedClicked();
    void copyTableRows(QTableView *tableView);
    void enableTableCopyAction(QTableView *tableView);

    // =========================================================================
    // SECTION: App Settings & Column Visibility
    // =========================================================================
    void onColumnsClicked();
    void onAutoScrollToggled(bool checked);
    void onFitRowsClicked();
    void onAppSettingsClicked();
    void applyColumnVisibility(const QVector<bool> &vis);
    void applyAppFont(const QFont &font);

    // =========================================================================
    // SECTION: Status Bar & Performance
    // =========================================================================
    void updateStatusBar();
    void updateMemoryUsage();
    void resizeVisibleRows();

    // =========================================================================
    // SECTION: Log Navigation Helpers
    // =========================================================================
    int findLogInAllLogs(const LogEntry &entry) const;
    int findLogInFilteredLogs(int allLogsIndex) const;
    int findNearestVisibleLog(int allLogsIndex) const;

    // =========================================================================
    // SECTION: Event Filter Helpers
    // =========================================================================
    bool handleShiftScrollEvent(QObject *obj, QWheelEvent *wheelEvent);
    bool handleCompleterFocusEvent(QObject *obj, QEvent *event);

    // =========================================================================
    // Data members
    // =========================================================================
    Ui::MainWindow  *m_ui;
    MainWindow      *m_mainWindow;

    // Devices tab: device row -> DeviceInfo map for click-to-select
    QMap<QWidget*, DeviceInfo> m_deviceRowMap;
    QWidget                   *m_selectedDeviceRow  = nullptr;
    QSet<QString>              m_checkedDevices;      // serials of checked devices

    // Log data
    QVector<LogEntry>    allLogs;
    QVector<LogEntry>    filteredLogs;
    quint64              m_nextLogId = 0;
    QHash<quint64, int>  m_allLogsIndex;
    QHash<quint64, int>  m_filteredLogsIndex;
    QSet<int>            m_markedRows;

    // Per-pane runtime UI state. Snapshotted on active-pane change so each
    // pane keeps its own filter / highlight / level radio values. Memory
    // only — discarded on app exit.
    struct PaneInputs {
        QString message;
        QString tag;
        QString package;
        QString pid;
        QString startTime;
        QString endTime;
        QString keyword;
        QString highlight;
        // 0=Verbose+, 1=V, 2=D, 3=I, 4=W, 5=E, 6=A, -1=none
        int     levelRadio = 0;
    };
    PaneInputs m_paneAInputs;
    PaneInputs m_paneBInputs;
    bool       m_lastActiveIsB = false;
    // When >= 0, forces useB() to that value (0 = pane A, 1 = pane B).
    // Used by sync-mode applyFilters to drive the inactive pane.
    int        m_paneOverride  = -1;
    // When true, filter / highlight / level changes apply to BOTH panes.
    bool       m_syncPanes     = false;
    void snapshotInputsTo(PaneInputs &out) const;
    void loadInputsFrom(const PaneInputs &in);

    // Models
    LogModel                *m_logModel               = nullptr;
    MarkLogModel            *m_markLogModel            = nullptr;
    SettingsModel           *m_settingsModel           = nullptr;
    PropertiesModel         *m_propertiesModel         = nullptr;
    PropertyDefinitionModel *m_propertyDefinitionModel = nullptr;

    // Misc data
    QVector<PropertyDefinition> m_availablePropertyDefinitions;
    QString                     m_currentDeviceId;

    // Input pipeline
    QVector<QString>  m_pendingLines;
    QTimer           *m_batchFlushTimer = nullptr;
    QTimer           *m_rowResizeTimer  = nullptr;

    // Background file loading
    QFutureWatcher<FileLoadResult> *m_fileLoaderWatcher = nullptr;
    bool                            m_isLoadingFile     = false;

    // State flags
    bool   isPaused    = true;
    
    qint64 memoryUsage = 0;
    int    m_highlightRow     = -1;
    int    m_pendingCenterRow = -1;

    // Converters / filters
    LogConverterPtr  m_logConverter;
    FileManager      m_fileManager;
    LogFilterController *m_logFilterController = nullptr;
    class ConfigurationController *m_configurationController = nullptr;

    // Status bar permanent indicators (live monitor + device count).
    class QLabel *m_lblStatusMonitor = nullptr;
    class QLabel *m_lblStatusDevices = nullptr;
    class QTimer *m_monitorPulseTimer = nullptr;
    bool m_monitorPulseBright = true;
    int m_monitorActiveCount = 0;

    // Theme: dark sheet captured from mainwindow.ui at startup.
    QString m_darkStylesheet;

    // Highlight delegates (pane A)
    HighlightDelegate *m_pidHighlightDelegate     = nullptr;
    HighlightDelegate *m_packageHighlightDelegate = nullptr;
    HighlightDelegate *m_tagHighlightDelegate     = nullptr;
    HighlightDelegate *m_messageHighlightDelegate = nullptr;
    // Highlight delegates (pane B) — created lazily when pane B is built so
    // that filter/highlight keywords only affect the active pane when sync
    // is off.
    HighlightDelegate *m_pidHighlightDelegateB     = nullptr;
    HighlightDelegate *m_packageHighlightDelegateB = nullptr;
    HighlightDelegate *m_tagHighlightDelegateB     = nullptr;
    HighlightDelegate *m_messageHighlightDelegateB = nullptr;

    // Filter history
    HistoryManager *m_historyManager = nullptr;

    // Cradle Manager tab controller (forward-declared to keep header light)
    class CradleController *m_cradleController = nullptr;
    class DumpsysController *m_dumpsysController = nullptr;
    class DevicesTabController *m_devicesTabController = nullptr;
    LogSplitController         *m_logSplitController   = nullptr;

    // ------------------------------------------------------------------------
    // Active-pane accessors. When the log split is active AND pane B is the
    // active pane, return references into LogSplitController::PaneState.
    // Otherwise return references to UiManager's pane-A members.
    // ------------------------------------------------------------------------
    inline bool useB() const {
        if (!m_logSplitController) return false;
        if (m_paneOverride >= 0)
            return m_paneOverride == 1
                   && m_logSplitController->paneB()
                   && m_logSplitController->paneB()->model;
        return m_logSplitController->activeIsB()
               && m_logSplitController->paneB() && m_logSplitController->paneB()->model;
    }
    inline QVector<LogEntry>&    activeAllLogs()      { return useB() ? m_logSplitController->paneB()->allLogs      : allLogs; }
    inline const QVector<LogEntry>& activeAllLogs() const { return useB() ? m_logSplitController->paneB()->allLogs : allLogs; }
    inline QVector<LogEntry>&    activeFilteredLogs() { return useB() ? m_logSplitController->paneB()->filteredLogs : filteredLogs; }
    inline const QVector<LogEntry>& activeFilteredLogs() const { return useB() ? m_logSplitController->paneB()->filteredLogs : filteredLogs; }
    inline QHash<quint64,int>&   activeAllLogsIndex() { return useB() ? m_logSplitController->paneB()->allLogsIndex : m_allLogsIndex; }
    inline const QHash<quint64,int>& activeAllLogsIndex() const { return useB() ? m_logSplitController->paneB()->allLogsIndex : m_allLogsIndex; }
    inline QHash<quint64,int>&   activeFilteredLogsIndex() { return useB() ? m_logSplitController->paneB()->filteredLogsIndex : m_filteredLogsIndex; }
    inline const QHash<quint64,int>& activeFilteredLogsIndex() const { return useB() ? m_logSplitController->paneB()->filteredLogsIndex : m_filteredLogsIndex; }
    inline QSet<int>&            activeMarkedRows()   { return useB() ? m_logSplitController->paneB()->markedRows   : m_markedRows; }
    inline quint64&              activeNextLogId()    { return useB() ? m_logSplitController->paneB()->nextLogId    : m_nextLogId; }
    inline LogModel*             activeLogModel()     { return useB() ? m_logSplitController->paneB()->model        : m_logModel; }
    inline MarkLogModel*         activeMarkLogModel() { return useB() ? m_logSplitController->paneB()->markModel    : m_markLogModel; }
    QTableView*                  activeTableLog();
    QTableView*                  activeTableMarkLog();
};

#endif // UIMANAGER_H
