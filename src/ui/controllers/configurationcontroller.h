#ifndef CONFIGURATIONCONTROLLER_H
#define CONFIGURATIONCONTROLLER_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <optional>

#include "configurationmanageroutput.h"
#include "propertyentry.h"
#include "propertysetjson.h"
#include "settingentry.h"
#include "sqlitepresetstore.h"

namespace Ui { class MainWindow; }
class QHBoxLayout;
class QMainWindow;
class QComboBox;
class QPoint;
class QPushButton;
class QTableView;
class QTimer;
class PropertyValueDelegate;
class RowActionDelegate;
class SettingsModel;
class PropertiesModel;
class PropertyDefinitionModel;

/**
 * Owns the Configuration tab and the SDK property workflows that
 * historically lived inside UiManager.
 *
 * Ownership:
 *   - Tables (Settings/Properties/PropertyDefinitions) and the models
 *     remain owned by UiManager.
 *   - All user-facing handlers live here.
 *
 * SDK properties: the whole configuration_manager catalog of the current
 * device is listed. Edits are staged in the model and written together with
 * Apply (or at once, with "Apply immediately" on); property sets save,
 * export and re-stage values.
 */
class ConfigurationController : public QObject
{
    Q_OBJECT
public:
    using DeviceIdProvider = std::function<QString()>;

    ConfigurationController(Ui::MainWindow *ui,
                            QMainWindow *mainWindow,
                            SettingsModel *settingsModel,
                            PropertiesModel *propertiesModel,
                            PropertyDefinitionModel *propertyDefinitionModel,
                            DeviceIdProvider deviceIdProvider,
                            QObject *parent = nullptr);

    void setupTables();
    void setupSDKTab();
    void setupMonitorButtons();

    /** Load the current device's property list, unless a load is running. */
    void refreshPropertyDefinitions();
    /** Forget the loaded property list and its staged changes. */
    void clearPropertyDefinitions();

public slots:
    void onRefreshSettingsClicked();
    void onRefreshPropertiesClicked();
    void onSettingsFetched(const QVector<SettingEntry> &settings);
    void onPropertiesFetched(const QVector<PropertyEntry> &properties);

    void onPropertyDefinitionsFetched(const QString &deviceId,
                                      const QVector<PropertyDefinition> &definitions,
                                      const QString &error);
    void onPropertyDefinitionsWritten(const QString &deviceId, bool reset,
                                      const PropertyWriteResult &result);

    // Property set persistence / exchange
    void onSavePropertySet();
    void onLoadPropertySet();
    void onExportPropertySet();
    void onImportPropertySet();

    void onSaveSettingClicked(int row);
    void onSettingSaveResult(int row, bool success,
                             const QString &group, const QString &setting,
                             const QString &newValue, const QString &verifiedValue,
                             const QString &error);
    void onSavePropertyClicked(int row);
    void onPropertySaveResult(int row, bool success,
                              const QString &property,
                              const QString &newValue, const QString &verifiedValue,
                              const QString &error);

    // Stop every active monitor (used by UiManager when the device changes /
    // disconnects — monitoring across a device switch makes no sense).
    void stopAllMonitors();

signals:
    // Emitted whenever any of the three monitor toggles change. The arg is the
    // count of currently-active monitors (0..3). Used by the status bar to
    // surface a persistent "live" indicator.
    void monitorStateChanged(int activeCount);
    // Per-table monitor state for visual border indication.
    void monitorTablesChanged(bool settings, bool properties, bool propertyDefs);

private:
    // -----------------------------------------------------------------------
    // Live monitor
    //
    // The Settings / Properties / Property-Definition tables each get a
    // "Monitor" toggle that re-issues its fetch on a tick. The three used to
    // be spelled out three times over; MonitorPane holds one pane's widgets
    // and state so a single set of helpers drives all of them.
    // -----------------------------------------------------------------------
    struct MonitorPane {
        QString      name;                  ///< user-facing table name
        QPushButton *button   = nullptr;
        QComboBox   *interval = nullptr;
        QTimer      *timer    = nullptr;
        bool         active   = false;
        bool         busy     = false;      ///< a fetch is already in flight
        /** Row actions disabled while the monitor writes into the table. */
        QVector<RowActionDelegate *> rowActions;
    };

    /** Build one monitor toggle + interval combo into @p row of the layout. */
    void buildMonitorControls(MonitorPane &pane, QHBoxLayout *row, QWidget *anchor);
    /** Shared toggle handler: start/stop the tick and refresh dependent UI. */
    void setMonitorActive(MonitorPane &pane, bool on);
    /** Emit monitorStateChanged / monitorTablesChanged from current state. */
    void publishMonitorState();

    /** Attach a paint-based action button to @p column of @p view. */
    RowActionDelegate *addRowAction(QTableView *view, int column,
                                    const QString &iconPath, const QString &tooltip,
                                    void (ConfigurationController::*slot)(int));

    bool validateDeviceId(const QString &deviceId) const;

    // ── SDK properties ───────────────────────────────────────────────────────
    /**
     * Write the staged changes. @p retryFailed also re-sends the ones whose
     * last write failed; automatic applying leaves those for the user.
     */
    void applyPropertyChanges(bool retryFailed);
    void discardPropertyChanges();
    void resetPropertiesToDefault(const QStringList &names);
    void showPropertyContextMenu(const QPoint &pos);
    void editPropertyValue(const QString &name);
    void applyPropertyFilterText();
    void updatePropertyActions();
    void updatePropertySummary();
    void updatePropertyFilterStatus();
    void updatePropertyDetails();
    /** Names of the selected rows, top to bottom. */
    QStringList selectedPropertyNames() const;

    // ── Property sets ────────────────────────────────────────────────────────
    /** True when a property list is loaded; tells the user otherwise. */
    bool requirePropertyCatalog(const QString &title) const;
    /** Ask which properties to save; nothing when cancelled or empty. */
    std::optional<QVector<PropertySetEntry>> choosePropertySet(const QString &title);
    /** Stage the values of a loaded set and report what happened. */
    void stagePropertySet(const QVector<PropertySetEntry> &entries, const QString &source);

    Ui::MainWindow          *m_ui;
    QMainWindow             *m_mainWindow;
    SettingsModel           *m_settingsModel;
    PropertiesModel         *m_propertiesModel;
    PropertyDefinitionModel *m_propertyDefinitionModel;
    SqlitePresetStore        m_propDefStore { QStringLiteral("propertydefs") };
    DeviceIdProvider         m_deviceIdProvider;

    PropertyValueDelegate   *m_propertyValueDelegate = nullptr;
    QTimer                  *m_propertyFilterTimer   = nullptr;
    QTimer                  *m_autoApplyTimer        = nullptr;
    QString                  m_propertyDeviceId;     ///< device the loaded list belongs to
    QString                  m_propertyLoadError;
    bool                     m_propertyFetchInFlight = false;
    bool                     m_propertyRefetch       = false;  ///< reload once the running load ends
    bool                     m_propertyWriteInFlight = false;
    QHash<QString, QString>  m_propertyWriteValues;  ///< values of the running `set`

    MonitorPane m_settingsMonitor;
    MonitorPane m_propertiesMonitor;
    MonitorPane m_propertyDefsMonitor;
};

#endif // CONFIGURATIONCONTROLLER_H
