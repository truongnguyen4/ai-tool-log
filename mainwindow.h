#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QVector>
#include <QString>
#include "adbmanager.h"
#include "ilogconverter.h"
#include "filemanager.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "ilogfilter.h"
#include "logfilter.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onFilterChanged();
    void onResumeClicked();
    void onClearClicked();
    void onCopyClicked();
    void onColumnsClicked();
    void onAutoScrollToggled(bool checked);
    void onAdbLogcatClicked();
    void onConfigurationClicked();
    void onSettingsClicked();
    void onDeviceChanged(int index);
    void updateStatusBar();
    void addSampleLogs();
    void onTableContextMenu(const QPoint &pos);
    void addToFilter(const QString &filterType, const QString &value, FilterOperator op);
    void onDevicesChanged(const QList<AdbDevice> &devices);
    void onLogcatLineReceived(const QString &line);
    void parseAndAddLogLine(const QString &line);
    void onLoadFileClicked();
    void onSaveFileClicked();
    void loadLogsFromFile(const QString &filePath);
    void onLogTableDoubleClicked(const QModelIndex &index);
    void onMarkLogTableClicked(const QModelIndex &index);

private:
    Ui::MainWindow *ui;
    QVector<LogEntry> allLogs;
    QVector<LogEntry> filteredLogs;
    LogModel *m_logModel;
    MarkLogModel *m_markLogModel;
    QSet<int> m_markedRows; // Track marked rows for highlighting
    bool isPaused;
    qint64 memoryUsage;
    LogConverterPtr m_logConverter;
    FileManager m_fileManager;
    LogFilter m_logFilter;
    
    void setupConnections();
    void applyFilters();
    void updateFilterCount();
    bool passesFilter(const LogEntry &entry);
    FilterCriteria buildFilterCriteria() const;
};
#endif // MAINWINDOW_H
