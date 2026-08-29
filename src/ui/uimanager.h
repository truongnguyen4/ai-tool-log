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
#include <QElapsedTimer>

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

#include <functional>

class QTableView;
class QWheelEvent;
class MainWindow;

// Timings shared by the ingest pipeline and the log views.
namespace UiTiming {
/** Coalescing window for incoming capture lines, in milliseconds. */
constexpr int kBatchFlushIntervalMs = 100;
/** Settling delay before word-wrapped rows are re-measured. */
constexpr int kRowResizeDebounceMs  = 150;
} // namespace UiTiming

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
    // SECTION: Nested value types
    // =========================================================================

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

    // Keywords to paint per column, derived from one pane's filter inputs.
    struct HighlightKeywords {
        QStringList message;
        QStringList tag;
        QStringList package;
        QStringList pid;
    };

    // Everything that belongs to one log pane, resolved in one place.
    //
    // Click and context-menu handlers must act on the pane the user actually
    // clicked in, which is not necessarily the active one. Resolving all the
    // per-pane members together keeps those handlers from each re-deriving the
    // same six ternaries — and from getting one of them wrong, as the
    // marked-log context menu did.
    struct PaneRefs {
        bool                isPaneB       = false;
        QVector<LogEntry>  *all           = nullptr;
        QVector<LogEntry>  *filtered      = nullptr;
        QHash<quint64,int> *allIndex      = nullptr;
        QHash<quint64,int> *filteredIndex = nullptr;
        QSet<int>          *markedRows    = nullptr;
        LogModel           *logModel      = nullptr;
        MarkLogModel       *markModel     = nullptr;
        QTableView         *logTable      = nullptr;
        QTableView         *markTable     = nullptr;
    };

    /** Resolve the pane that owns @p senderObject (falls back to pane A). */
    PaneRefs paneRefsForSender(const QObject *senderObject);

    // =========================================================================
    // SECTION: Setup — initialise visual components
    // =========================================================================
    void setupMainNavigationTabs();
    void setupLogTable();
    /** Default column widths for a table showing LogEntry rows. */
    void applyLogColumnWidths(QTableView *view);
    /** Install the keyword-highlight delegates for one pane's log table. */
    void installLogHighlightDelegates(QTableView *view, bool paneB);
    /**
     * Re-measure word-wrapped rows when the view scrolls or a text column is
     * resized. @p hasRows lets the caller skip the work on an empty pane.
     */
    void wireRowResizeTriggers(QTableView *view, std::function<bool()> hasRows);
    /** Finish wiring pane B once LogSplitController has created its widgets. */
    void onPaneBBuilt(QTableView *logTable, QTableView *markTable);
    void setupConfigurationTables();
    void convertSdkTabsToSidebar();
    void setupTabAutoFetch();
    void setupTooltips();
    void setupToolbarDividers();
    void setupStatusBarIndicators();
    void setupSplittersAndMisc();

    // Persist column widths + splitter sizes between sessions.
    void saveLayoutPreferences();
    void restoreLayoutPreferences();
    /** Splitters whose geometry survives a restart. */
    QList<QSplitter *> persistedSplitters() const;
    /** Tables whose header layout survives a restart. */
    QList<QTableView *> persistedTables() const;

    // =========================================================================
    // SECTION: Signal connections — group by feature area
    // =========================================================================
    void connectAdbManagerSignals();
    void connectFilterSignals();
    void connectButtonSignals();
    void connectTableSignals();
    /** Wire one pane's log + mark tables; shared by pane A and pane B. */
    void connectLogTableSignals(QTableView *logTable, QTableView *markTable);

    // =========================================================================
    // SECTION: Filter & Highlight
    // =========================================================================
    void onFilterChanged();
    void onHighlightChanged();
    /** Jump to the next (+1) or previous (-1) row matching the highlight box. */
    void navigateHighlight(int direction);
    void onHighlightNextClicked();
    void onHighlightPrevClicked();
    void updateFilterHighlighting();
    HighlightKeywords collectHighlightKeywords(const PaneInputs &inputs) const;
    void applyHighlightKeywords(const HighlightKeywords &keywords, bool paneB);
    void onSettingsFilterChanged();
    void onPropertiesFilterChanged();
    FilterCriteria buildFilterCriteria() const;
    void applyFilters();
    void updateFilterCount();
    bool passesFilter(const LogEntry &entry);
    void setupFilterCompleters();

    // =========================================================================
    // SECTION: Device / Capture
    //
    // Logcat and kernel logs share one ingest pipeline; they differ only in
    // the ILogConverter installed when the capture starts.
    // =========================================================================
    void onDeviceChanged(int index);
    void onDevicesChanged(const QList<AdbDevice> &devices);
    /** Colour the device indicator; the palette lives in the theme sheet. */
    void setDeviceStatusConnected(bool connected);
    /** Accent a configuration table while its live monitor is polling. */
    void setTableMonitoring(QTableView *view, bool monitoring);
    /** Queue one raw capture line for the next batch flush. */
    void onLogLineReceived(const QString &line);
    void onStartClicked();
    void onKernelClicked();
    void onClearClicked();
    void flushPendingLines();

    /** Drop every log, mark and index belonging to the active pane. */
    void resetActivePaneLogs();
    /** Reset the active pane and install @p converter. False if no device. */
    bool beginCapture(const LogConverterPtr &converter);

    // Status-bar feedback helpers — single source of truth for transient
    // toolbar messages. Kept side-by-side with the logcat slots so all
    // message-emitting code reads the same way (UiManager::flashStatus(...)).
    void flashStatus(const QString &message);

    /** Flush any queued lines and refresh the status bar after a capture ends. */
    void stopCapture();

    // Toolbar visuals for the logcat / kernel toggle pair. Both capture
    // sources share one buffer, so starting either disables the other.
    void setCaptureButtonState(QPushButton *active, QPushButton *other, bool running);
    void setLogcatRunningVisuals(bool running);
    void setKernelRunningVisuals(bool running);

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
    void onMarkLogTableClicked(const QModelIndex &index);
    void onMarkLogContextMenu(const QPoint &pos);
    void onClearAllMarkedClicked();
    void copyTableRows(QTableView *tableView);
    void enableTableCopyAction(QTableView *tableView);

    // =========================================================================
    // SECTION: App Settings & Column Visibility
    // =========================================================================
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
    /** Row of @p allLogsIndex in the active pane's filtered view, or -1. */
    int findLogInFilteredLogs(int allLogsIndex) const;

    // =========================================================================
    // SECTION: Event Filter Helpers
    // =========================================================================
    /** Shift + wheel scrolls a log table horizontally, in either pane. */
    bool handleShiftScrollEvent(QObject *obj, QWheelEvent *wheelEvent);
    bool handleCompleterFocusEvent(QObject *obj, QEvent *event);
    /** Devices tab: select the clicked device row, in either device list. */
    void handleDeviceRowClick(QWidget *row);
    void highlightSelectedDeviceRow(QWidget *selected);

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

    // Input pipeline. Incoming logcat and dmesg lines are queued here and
    // flushed into the model in batches, so a noisy device costs one model
    // insert per flush instead of one per line.
    QVector<QString>  m_pendingLines;
    QTimer           *m_batchFlushTimer = nullptr;
    QTimer           *m_rowResizeTimer  = nullptr;

    // Configuration-tab filters run on every keystroke; debounce them so a
    // fast typist doesn't trigger a full re-filter per character.
    QTimer *m_settingsFilterTimer   = nullptr;
    QTimer *m_propertiesFilterTimer = nullptr;

    // Background file loading
    QFutureWatcher<FileLoadResult> *m_fileLoaderWatcher = nullptr;
    bool                            m_isLoadingFile     = false;

    // Resident-set size shown in the status bar, in MiB. Sampling it means
    // reading from the OS, so the value is refreshed on a timer rather than
    // on every status-bar update.
    qint64        m_memoryUsageMb = 0;
    QElapsedTimer m_memorySampleClock;

    int m_highlightRow     = -1;
    int m_pendingCenterRow = -1;

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
};

#endif // UIMANAGER_H
