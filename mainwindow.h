#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QMap>
#include "adbmanager.h"
#include "ilogconverter.h"
#include "filemanager.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "settingsmodel.h"
#include "propertiesmodel.h"
#include "propertydefinitionmodel.h"
#include "valuedelegate.h"
#include "ilogfilter.h"
#include "logfilter.h"
#include "QLineEdit"

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
    void onSettingsFilterChanged();
    void onPropertiesFilterChanged();
    void onRefreshSettingsClicked();
    void onRefreshPropertiesClicked();
    void onResumeClicked();
    void onClearClicked();
    void onCopyClicked();
    void onColumnsClicked();
    void onAutoScrollToggled(bool checked);
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
    void onOpenFileClicked();
    void onSaveFileClicked();
    void loadLogsFromFile(const QString &filePath);
    void onLogTableDoubleClicked(const QModelIndex &index);
    void onMarkLogTableClicked(const QModelIndex &index);
    void onSettingsFetched(const QVector<SettingEntry> &settings);
    void onPropertiesFetched(const QVector<PropertyEntry> &properties);
    void onSaveSettingClicked(int row);
    void onSavePropertyClicked(int row);
    
    // SDK tab slots
    void onSearchPropertyDefinition();
    void onAddPropertyDefinition();
    void onClearAllPropertyDefinitions();
    void onFetchPropertyDefinitions();
    void onPropertyDefinitionsFetched(const QVector<PropertyDefinition> &propertyDefinitions);
    void onGetPropertyDefinitionClicked(int row);
    void onSetPropertyDefinitionClicked(int row);
    void onRemovePropertyDefinitionClicked(int row);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Ui::MainWindow *ui;
    QVector<LogEntry> allLogs;
    QVector<LogEntry> filteredLogs;
    LogModel *m_logModel;
    MarkLogModel *m_markLogModel;
    SettingsModel *m_settingsModel;
    PropertiesModel *m_propertiesModel;
    PropertyDefinitionModel *m_propertyDefinitionModel;
    QVector<PropertyDefinition> m_availablePropertyDefinitions; // All available property definitions for auto-complete
    QSet<int> m_markedRows; // Track marked rows for highlighting
    QString m_currentDeviceId; // Currently selected device
    bool isPaused;
    qint64 memoryUsage;
    LogConverterPtr m_logConverter;
    FileManager m_fileManager;
    LogFilter m_logFilter;
    
    // Filter history management
    QMap<QLineEdit*, QStringList> m_filterHistory;
    QMap<QLineEdit*, int> m_historyIndex;
    QMap<QLineEdit*, QString> m_currentText; // Store text being typed before navigating history
    static const int MAX_HISTORY_SIZE = 50;
    
    void setupConnections();
    void setupConfigurationTables();
    void setupSDKTab();
    void recreatePropertyDefinitionButtons();
    void recreateSettingsButtons();
    void recreatePropertiesButtons();
    void createSampleSettings();
    void createSampleProperties();
    void createSamplePropertyDefinitions();
    void updatePropertyNamesCompleter();
    void applyFilters();
    void updateFilterCount();
    bool passesFilter(const LogEntry &entry);
    FilterCriteria buildFilterCriteria() const;
    void saveToHistory(QLineEdit *lineEdit);
    void navigateHistory(QLineEdit *lineEdit, bool up);
};
#endif // MAINWINDOW_H
