// DevicesTabController: owns the wiring of the "Devices" tab UI.
// Friend of UiManager so it can drive its private members directly.
#pragma once

#include "uimanager.h"
#include <QObject>
#include <QStringList>

class QWidget;
struct DeviceDetails;

class DevicesTabController : public QObject
{
    Q_OBJECT
public:
    explicit DevicesTabController(UiManager *owner, QObject *parent = nullptr);

    // Wires DevicesManager signals and Devices-tab buttons. Call once during
    // UiManager::initialize().
    void setup();

    // Wires the Configuration sub-tab (toggles, JSON, presets, deploy). Called
    // from setup().
    void setupConfigTab();

    // Wraps the dashboard sections (sysinfo, stats, wifi, firmware, quick
    // actions, configuration) into styled QFrame cards. Called once from
    // setup() after all widgets exist.
    void polishDashboardCards();

    // Devices tab runtime
    void refreshDevicesTab();
    void refreshCheckedDevicesList();
    void onDevicesOrGroupsChanged();
    void selectDeviceRow(QWidget *row, const UiManager::DeviceInfo &info);
    void updateDeviceDetails(const UiManager::DeviceInfo &info);
    void onDeviceDetailsFetched(const DeviceDetails &details);

private:
    // Returns target serials: all checked online devices, or the single
    // currently-selected device as fallback. Empty when nothing is selected.
    QStringList selectedSerials() const;

    // Convenience: dispatch an AdbCommand factory (e.g. AdbCommand::rebootDevice)
    // to every selected serial via DevicesManager. Used to collapse the
    // repeated for-loop boilerplate in quick-action button slots.
    void runOnSelected(QStringList (*adbCommandFactory)(const QString &));

    // Spawns the firmware-flash progress dialog and runs `download.sh` in
    // parallel for each selected serial. Validates inputs (selection, path,
    // script presence) and warns the user via QMessageBox on failure.
    void runFlashFirmware();

    UiManager *m_owner = nullptr;
};
