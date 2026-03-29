#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QList>
#include <QFutureWatcher>
#include <QPushButton>

#include "adbmanager.h"
#include "ilogconverter.h"
#include "filemanager.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "filterhistorymanager.h"
#include "highlightdelegate.h"
#include "ilogfilter.h"
#include "logfilter.h"
#include "propertydefinitionbackend.h"
#include "socketserver.h"
#include "settingssockethandler.h"
#include "systempropertysockethandler.h"
#include "propertydefinitionsockethandler.h"

class QTermWidget;
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

public:
    explicit UiManager(Ui::MainWindow *ui, MainWindow *mainWindow);

    // Called once by MainWindow::MainWindow() after ui->setupUi().
    void initialize();

    // Delegated from MainWindow::eventFilter().
    bool handleEvent(QObject *obj, QEvent *event);

    // Called from MainWindow::closeEvent() to flush filter history to QSettings.
    void persistFilterHistory();
    // Called from MainWindow::closeEvent() to disconnect socket and remove adb port forwarding.
    void teardownSocket();

private:
    // =========================================================================
    // SECTION: Setup — initialise visual components
    // =========================================================================
    void setupLogTable();
    void setupConfigurationTables();
    void setupSDKTab();
    void setupTerminal();
    void setupDumpsys();
    void setupCradleTab();
    void setupTooltips();
    void setupFilterHistory();
    void setupSplittersAndMisc();

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
    void onHighlightNext();
    void onHighlightPrev();
    void updateFilterHighlighting();
    void onSettingsFilterChanged();
    void onPropertiesFilterChanged();
    FilterCriteria buildFilterCriteria() const;
    void applyFilters();
    void updateFilterCount();
    bool passesFilter(const LogEntry &entry);

    // =========================================================================
    // SECTION: Device / Logcat
    // =========================================================================
    void onDeviceChanged(int index);
    void onDevicesChanged(const QList<AdbDevice> &devices);
    void onLogcatLineReceived(const QString &line);
    void onStartClicked();
    void onClearClicked();
    void flushPendingLines();

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
    // SECTION: Configuration Tab (Settings & Properties)
    // =========================================================================
    void onRefreshSettingsClicked();
    void onRefreshPropertiesClicked();
    void onSettingsFetched(const QVector<SettingEntry> &settings);
    void onPropertiesFetched(const QVector<PropertyEntry> &properties);
    void onSaveSettingClicked(int row);
    void onSavePropertyClicked(int row);
    void onSettingSaveResult(int row, bool success,
                             const QString &group, const QString &setting,
                             const QString &newValue, const QString &verifiedValue,
                             const QString &error);
    void onPropertySaveResult(int row, bool success,
                              const QString &property,
                              const QString &newValue, const QString &verifiedValue,
                              const QString &error);
    void recreateSettingsButtons();
    void recreatePropertiesButtons();
    static QPushButton *createActionButton(const QString &label,
                                           const QString &tooltip,
                                           int maxWidth,
                                           QWidget *parent);

    // =========================================================================
    // SECTION: SDK Tab (Property Definitions)
    // =========================================================================
    void onSearchPropertyDefinition();
    void onAddPropertyDefinition();
    void onClearAllPropertyDefinitions();
    void onFetchPropertyDefinitions();
    void onRefreshPropertyDefinitionValues();
    void onPropertyDefinitionsFetched(const QVector<PropertyDefinition> &defs);
    void onGetPropertyDefinitionClicked(int row);
    void onSetPropertyDefinitionClicked(int row);
    void onRemovePropertyDefinitionClicked(int row);
    void updatePropertyNamesCompleter();

    // Property set persistence / exchange
    void onSavePropertySet();
    void onLoadPropertySet();
    void onExportPropertySet();
    void onImportPropertySet();

    // Recreate Set/Get/Remove button widgets for all rows in the property definition table.
    // Must be called after any bulk model reset (load from DB, import from file).
    void recreatePropertyDefinitionButtons();

    // =========================================================================
    // SECTION: Dumpsys Tab
    // =========================================================================
    void onRunDumpsysClicked();
    void onDumpsysFetched(const QString &output);
    void onDumpsysListFetched(const QStringList &services);
    void onDumpsysSearchChanged();
    void onDumpsysSearchNext();
    void onDumpsysSearchPrev();
    void applyDumpsysHighlights(const QString &needle);
    void updateDumpsysCommandText();
    void onRunDumpsysCmdClicked();
    void onRawAdbCommandFinished(const QString &output);

    // =========================================================================
    // SECTION: Cradle Manager Tab
    // =========================================================================
    void onCradleGetInfo();
    void onCradleQueryFirmware();
    void onCradleUpdateFirmware();
    void onCradleQuerySchedule();
    void onCradleCommandFinished(const QString &output, const QString &error);

    // =========================================================================
    // SECTION: Terminal
    // =========================================================================
    void onToggleTerminal(bool checked);

    // =========================================================================
    // SECTION: Socket Listener
    // =========================================================================
    void setupSocketListener();

    // =========================================================================
    // SECTION: Table Interaction
    // =========================================================================
    void onTableContextMenu(const QPoint &pos);
    void addToFilter(const QString &filterType, const QString &value, FilterOperator op);
    void onLogTableDoubleClicked(const QModelIndex &index);
    void onLogTableClicked(const QModelIndex &index);
    void onMarkLogTableClicked(const QModelIndex &index);
    void onMarkLogContextMenu(const QPoint &pos);
    void onClearAllMarkedLog();
    void showCellContent(QTableView *tableView,
                         const QAbstractItemModel *model,
                         const QModelIndex &index);
    void copyTableRows(QTableView *tableView);
    void enableTableCopyAction(QTableView *tableView);

    // =========================================================================
    // SECTION: App Settings & Column Visibility
    // =========================================================================
    void onColumnsClicked();
    void onAutoScrollToggled(bool checked);
    void onAppSettingsClicked();
    void applyColumnVisibility(const QVector<bool> &vis);
    void applyPropDefColumnVisibility(const QVector<bool> &vis);
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
    bool handleTerminalKeyEvent(QObject *obj, QEvent *event);
    bool handleShiftScrollEvent(QObject *obj, QWheelEvent *wheelEvent);
    bool handleCompleterFocusEvent(QObject *obj, QEvent *event);

    // =========================================================================
    // Data members
    // =========================================================================
    Ui::MainWindow  *m_ui;
    MainWindow      *m_mainWindow;

    // Log data
    QVector<LogEntry>    allLogs;
    QVector<LogEntry>    filteredLogs;
    quint64              m_nextLogId = 0;
    QHash<quint64, int>  m_allLogsIndex;
    QHash<quint64, int>  m_filteredLogsIndex;
    QSet<int>            m_markedRows;

    // Models
    LogModel                *m_logModel               = nullptr;
    MarkLogModel            *m_markLogModel            = nullptr;
    SettingsModel           *m_settingsModel           = nullptr;
    PropertiesModel         *m_propertiesModel         = nullptr;
    PropertyDefinitionModel *m_propertyDefinitionModel = nullptr;

    // Misc data
    QVector<PropertyDefinition> m_availablePropertyDefinitions;
    QStringList                 m_dumpsysServices;
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
    LogFilter        m_logFilter;

    // Highlight delegates
    HighlightDelegate *m_pidHighlightDelegate     = nullptr;
    HighlightDelegate *m_packageHighlightDelegate = nullptr;
    HighlightDelegate *m_tagHighlightDelegate     = nullptr;
    HighlightDelegate *m_messageHighlightDelegate = nullptr;

    // Filter history
    FilterHistoryManager *m_filterHistoryManager = nullptr;

    // Property definition persistence / export backend
    PropertyDefinitionBackend *m_propDefBackend = nullptr;

    // Embedded terminal
    QTermWidget *m_terminal          = nullptr;
    QList<int>   m_savedSplitterSizes;

    // Socket listener for live settings/property updates from device
    SocketServer                        *m_socketServer                        = nullptr;
    SettingsSocketHandler               *m_settingsSocketHandler               = nullptr;
    SystemPropertySocketHandler         *m_systemPropertySocketHandler         = nullptr;
    PropertyDefinitionSocketHandler     *m_propertyDefinitionSocketHandler     = nullptr;
};

#endif // UIMANAGER_H
