#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QVector>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QPushButton>
#include <QLineEdit>
#include <QFutureWatcher>
#include "adbmanager.h"
#include "ilogconverter.h"
#include "filemanager.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "filterhistorymanager.h"
#include "valuedelegate.h"
#include "highlightdelegate.h"
#include "ilogfilter.h"
#include "logfilter.h"

// Result bundle produced by the background file-loading thread.
// IDs are assigned on the worker thread so the main thread only needs
// to std::move the data into place (O(1) transfers).
struct FileLoadResult {
    QVector<LogEntry>       entries;      // parsed entries with IDs already set
    QHash<quint64, int>     allLogsIndex; // id → index in entries (pre-built)
    quint64                 nextLogId = 0;
    LogConverterPtr         converter;
    QString                 errorMsg;
    QString                 filePath;
    int                     parsedCount = 0;
    int                     lineCount   = 0;
};

class QTermWidget;
class QSplitter;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class QTableView;
class QWheelEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Filtering / highlighting
    void onFilterChanged();
    void onHighlightChanged();
    void onHighlightNext();
    void onHighlightPrev();
    void updateFilterHighlighting();
    void onSettingsFilterChanged();
    void onPropertiesFilterChanged();

    // Device / logcat
    void onDeviceChanged(int index);
    void onDevicesChanged(const QList<AdbDevice> &devices);
    void onLogcatLineReceived(const QString &line);
    void onStartClicked();
    void onClearClicked();

    // File I/O
    void onLoadFileClicked();
    void onOpenFileClicked();
    void onSaveFileClicked();
    void onFileLoadFinished();   // called when background file-load completes

    // Config tab
    void onRefreshSettingsClicked();
    void onRefreshPropertiesClicked();
    void onSettingsFetched(const QVector<SettingEntry> &settings);
    void onPropertiesFetched(const QVector<PropertyEntry> &properties);
    void onSaveSettingClicked(int row);
    void onSavePropertyClicked(int row);
    // Async save results (Issue #7)
    void onSettingSaveResult(int row, bool success,
                             const QString &group, const QString &setting,
                             const QString &newValue, const QString &verifiedValue,
                             const QString &error);
    void onPropertySaveResult(int row, bool success,
                              const QString &property,
                              const QString &newValue, const QString &verifiedValue,
                              const QString &error);

    // SDK tab
    void onSearchPropertyDefinition();
    void onAddPropertyDefinition();
    void onClearAllPropertyDefinitions();
    void onFetchPropertyDefinitions();
    void onRefreshPropertyDefinitionValues();
    void onPropertyDefinitionsFetched(const QVector<PropertyDefinition> &propertyDefinitions);
    void onGetPropertyDefinitionClicked(int row);
    void onSetPropertyDefinitionClicked(int row);
    void onRemovePropertyDefinitionClicked(int row);

    // Dumpsys tab
    void onRunDumpsysClicked();
    void onDumpsysFetched(const QString &output);
    void onDumpsysListFetched(const QStringList &services);
    void onDumpsysSearchChanged();
    void onDumpsysSearchNext();
    void onDumpsysSearchPrev();
    void applyDumpsysHighlights(const QString &needle);

    // Cradle Manager tab
    void onCradleGetInfo();
    void onCradleQueryFirmware();
    void onCradleUpdateFirmware();
    void onCradleQuerySchedule();
    void onCradleCommandFinished(const QString &output, const QString &error);

    // UI helpers
    void onColumnsClicked();
    void onAutoScrollToggled(bool checked);
    void onTableContextMenu(const QPoint &pos);
    void addToFilter(const QString &filterType, const QString &value, FilterOperator op);
    void onLogTableDoubleClicked(const QModelIndex &index);
    void onLogTableClicked(const QModelIndex &index);
    void onMarkLogTableClicked(const QModelIndex &index);
    void onMarkLogContextMenu(const QPoint &pos);
    void onClearAllMarkedLog();
    void updateStatusBar();

    // Terminal
    void onToggleTerminal(bool checked);

    // Kernel (dmesg)
    void onKernelClicked();

    // Application settings
    void onAppSettingsClicked();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // -----------------------------------------------------------------------
    // Setup helpers
    // -----------------------------------------------------------------------
    void setupConnections();
    void setupConfigurationTables();
    void setupSDKTab();
    void setupTerminal();
    void setupDumpsys();
    void setupCradleTab();
    void setupTooltips();
    void applyAppFont(const QFont &font);
    // Apply column visibility to both tables and sync dependent filter group boxes
    // groupBox_2 (Package), groupBox_3 (PID), groupBox_6 (Time)
    void applyColumnVisibility(const QVector<bool> &vis);
    void applyPropDefColumnVisibility(const QVector<bool> &vis);

    // -----------------------------------------------------------------------
    // Button factories (Issue #4 – single place for action-button creation)
    // -----------------------------------------------------------------------
    static QPushButton* createActionButton(const QString &label,
                                           const QString &tooltip,
                                           int maxWidth,
                                           QWidget *parent);
    void recreateSettingsButtons();
    void recreatePropertiesButtons();

    // -----------------------------------------------------------------------
    // Auto-complete
    // -----------------------------------------------------------------------
    void updatePropertyNamesCompleter();

    // -----------------------------------------------------------------------
    // Filtering
    // -----------------------------------------------------------------------
    void applyFilters();
    void updateFilterCount();
    bool passesFilter(const LogEntry &entry);
    FilterCriteria buildFilterCriteria() const;

    // -----------------------------------------------------------------------
    // Log ingestion helpers
    // -----------------------------------------------------------------------
    void flushPendingLines();          // drain m_pendingLines into model in one batch
    void loadLogsFromFile(const QString &filePath);

    // -----------------------------------------------------------------------
    // Mark / navigation helpers  (Issue #6 – O(1) id-based lookup)
    // -----------------------------------------------------------------------
    int findLogInAllLogs(const LogEntry &entry) const;
    int findLogInFilteredLogs(int allLogsIndex) const;
    int findNearestVisibleLog(int allLogsIndex) const;

    // -----------------------------------------------------------------------
    // Table UI helpers (Issue #5 – shared cell-display logic)
    // -----------------------------------------------------------------------
    void showCellContent(QTableView *tableView,
                         const QAbstractItemModel *model,
                         const QModelIndex &index);

    // -----------------------------------------------------------------------
    // eventFilter decomposition  (Issue #9)
    // -----------------------------------------------------------------------
    bool handleShiftScrollEvent(QObject *obj, QWheelEvent *wheelEvent);
    bool handleCompleterFocusEvent(QObject *obj, QEvent *event);

    // -----------------------------------------------------------------------
    // Copy helpers
    // -----------------------------------------------------------------------
    void copyTableRows(QTableView *tableView);

    // -----------------------------------------------------------------------
    // Dmesg (Kernel log) helpers
    // -----------------------------------------------------------------------
    void parseDmesgLine(const QString &line);

    // -----------------------------------------------------------------------
    // Table UI helpers – performance
    // -----------------------------------------------------------------------
    // Resize only the rows currently visible in the viewport so we avoid the
    // O(total-rows) cost of QTableView::resizeRowsToContents() when the model
    // contains hundreds of thousands of entries.
    void resizeVisibleRows();

    // -----------------------------------------------------------------------
    // Status bar
    // -----------------------------------------------------------------------
    void updateMemoryUsage();  // Issue #11 – reads /proc/self/status

    // -----------------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------------
    Ui::MainWindow *ui;

    QVector<LogEntry> allLogs;
    QVector<LogEntry> filteredLogs;

    // Issue #6: O(1) id-keyed indexes
    quint64              m_nextLogId = 0;
    QHash<quint64, int>  m_allLogsIndex;      // id -> index in allLogs
    QHash<quint64, int>  m_filteredLogsIndex; // id -> index in filteredLogs

    LogModel                *m_logModel;
    MarkLogModel            *m_markLogModel;
    SettingsModel           *m_settingsModel;
    PropertiesModel         *m_propertiesModel;
    PropertyDefinitionModel *m_propertyDefinitionModel;

    QVector<PropertyDefinition> m_availablePropertyDefinitions;
    QStringList                 m_dumpsysServices;
    QSet<int>                   m_markedRows;

    QString m_currentDeviceId;


    // Live logcat batching (100 ms flush) for smooth UI under high log volume
    QVector<QString> m_pendingLines;
    QTimer          *m_batchFlushTimer = nullptr;
    // Debounce timer so resizeRowsToContents() isn't called on every pixel
    // during column resize or every batch flush (fires after 150 ms of quiet).
    QTimer          *m_rowResizeTimer  = nullptr;

    // Background file loading
    QFutureWatcher<FileLoadResult> *m_fileLoaderWatcher = nullptr;
    bool             m_isLoadingFile  = false;

    bool    isPaused     = true;
    qint64  memoryUsage  = 0;
    int     m_highlightRow     = -1; // current row for highlight find navigation
    int     m_pendingCenterRow = -1; // row to re-center after resizeVisibleRows

    LogConverterPtr m_logConverter;
    FileManager     m_fileManager;
    LogFilter       m_logFilter;

    // Highlight delegates
    HighlightDelegate *m_pidHighlightDelegate;
    HighlightDelegate *m_packageHighlightDelegate;
    HighlightDelegate *m_tagHighlightDelegate;
    HighlightDelegate *m_messageHighlightDelegate;

    // Issue #1 / #9 – filter history extracted to its own class
    FilterHistoryManager *m_filterHistoryManager;

    // Embedded terminal
    QTermWidget *m_terminal = nullptr;
    QList<int>   m_savedSplitterSizes;   // remember sizes when hiding terminal

    // Dmesg (kernel log) — process managed by AdbManager
};

#endif // MAINWINDOW_H
