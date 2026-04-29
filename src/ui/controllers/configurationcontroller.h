#ifndef CONFIGURATIONCONTROLLER_H
#define CONFIGURATIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

#include "propertydefinition.h"
#include "settingentry.h"
#include "propertyentry.h"
#include "sqlitepresetstore.h"
#include <QStringList>

namespace Ui { class MainWindow; }
class QMainWindow;
class QComboBox;
class QPushButton;
class QTableView;
class QTimer;
class SettingsModel;
class PropertiesModel;
class PropertyDefinitionModel;

/**
 * Owns the Configuration tab + Property Definition (SDK) workflows that
 * historically lived inside UiManager.
 *
 * Ownership:
 *   - Tables (Settings/Properties/PropertyDefinitions) and the models
 *     remain owned by UiManager.
 *   - All user-facing handlers and the "available property definitions"
 *     cache live here.
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
    void applyPropDefColumnVisibility(const QVector<bool> &vis);
    void updatePropertyNamesCompleter();
    void recreatePropertyDefinitionButtons();
    void clearAvailableCache();

    const QVector<PropertyDefinition>& availablePropertyDefinitions() const
    { return m_availablePropertyDefinitions; }

public slots:
    void onRefreshSettingsClicked();
    void onRefreshPropertiesClicked();
    void onSettingsFetched(const QVector<SettingEntry> &settings);
    void onPropertiesFetched(const QVector<PropertyEntry> &properties);

    // Property set persistence / exchange
    void onSavePropertySet();
    void onLoadPropertySet();
    void onExportPropertySet();
    void onImportPropertySet();

    void onSearchPropertyDefinition();
    void onAddPropertyDefinition();
    void onClearAllPropertyDefinitions();
    void onFetchPropertyDefinitions();
    void onRefreshPropertyDefinitionValues();
    void onPropertyDefinitionsFetched(const QVector<PropertyDefinition> &defs);
    void onGetPropertyDefinitionClicked(int row);
    void onSetPropertyDefinitionClicked(int row);
    void onRemovePropertyDefinitionClicked(int row);

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
    void recreateSettingsButtons();
    void recreatePropertiesButtons();

    // Monitor toggles — periodically re-fetch the corresponding table every
    // 500 ms and disable per-row write buttons while active.
    void onMonitorSettingsToggled(bool on);
    void onMonitorPropertiesToggled(bool on);
    void onMonitorPropertyDefsToggled(bool on);

    bool isMonitoringSettings()      const { return m_monitoringSettings; }
    bool isMonitoringProperties()    const { return m_monitoringProperties; }
    bool isMonitoringPropertyDefs()  const { return m_monitoringPropertyDefs; }

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
    void wirePropertyRowButtons(int row);
    void setSettingsRowActionsEnabled(bool enabled);
    void setPropertiesRowActionsEnabled(bool enabled);
    void setPropertyDefSetButtonsEnabled(bool enabled);
    
    // Helper methods to reduce code duplication
    PropertyDefinition findPropertyByName(const QString &name) const;
    PropertyDefinition parsePropertyDefinition(const QString &output, const QString &fallbackName = QString()) const;
    bool validateDeviceId(const QString &deviceId) const;

    Ui::MainWindow          *m_ui;
    QMainWindow             *m_mainWindow;
    SettingsModel           *m_settingsModel;
    PropertiesModel         *m_propertiesModel;
    PropertyDefinitionModel *m_propertyDefinitionModel;
    SqlitePresetStore        m_propDefStore { QStringLiteral("propertydefs") };
    DeviceIdProvider         m_deviceIdProvider;

    QVector<PropertyDefinition> m_availablePropertyDefinitions;

    // Monitor state.
    QPushButton *m_btnMonitorSettings     = nullptr;
    QPushButton *m_btnMonitorProperties   = nullptr;
    QPushButton *m_btnMonitorPropertyDefs = nullptr;
    QComboBox   *m_cmbIntervalSettings     = nullptr;
    QComboBox   *m_cmbIntervalProperties   = nullptr;
    QComboBox   *m_cmbIntervalPropertyDefs = nullptr;
    QTimer      *m_monitorSettingsTimer     = nullptr;
    QTimer      *m_monitorPropertiesTimer   = nullptr;
    QTimer      *m_monitorPropertyDefsTimer = nullptr;
    bool m_monitoringSettings     = false;
    bool m_monitoringProperties   = false;
    bool m_monitoringPropertyDefs = false;
    // Re-entrancy guards for the per-row monitor refresh tasks. Set while a
    // QtConcurrent refresh batch is in flight; the next timer tick is skipped
    // if the previous batch hasn't completed yet.
    bool m_settingsRefreshBusy     = false;
    bool m_propertiesRefreshBusy   = false;
    bool m_propertyDefsRefreshBusy = false;
};

#endif // CONFIGURATIONCONTROLLER_H
