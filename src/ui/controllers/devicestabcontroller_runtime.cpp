#include "devicestabcontroller.h"
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "devicesmanager.h"
#include "colorscheme.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

void DevicesTabController::refreshDevicesTab()
{
    UiManager *uim = m_owner;
    const ColorScheme &cs = ColorScheme::instance();
    const QString colText   = ColorScheme::toHex(cs.text());
    const QString colMuted  = ColorScheme::toHex(cs.mutedText());
    const QString colAccent = ColorScheme::toHex(cs.accent());
    const QString colGreen  = ColorScheme::toHex(cs.success());
    const QString colBorder = ColorScheme::toHex(cs.border());
    const QString colRowSel = ColorScheme::toHex(cs.rowSelectedBackground());

    // ── Device row factory (with checkbox) ────────────────────────────────────
    //
    // Rows carry a `selected` dynamic property and are styled by the theme
    // sheet's QWidget[deviceRow="true"] rules. Building a per-row stylesheet
    // string, as this used to, meant re-parsing QSS once per device on every
    // refresh — and duplicated the checkbox styling the theme already defines.
    auto makeDeviceRow = [&](const UiManager::DeviceInfo &info, bool selected, bool checked) -> QWidget* {
        auto *row = new QWidget();
        row->setObjectName(QStringLiteral("devRow_") + info.serial);
        row->setProperty("deviceRow", true);
        row->setProperty("selected", selected);
        row->setCursor(Qt::PointingHandCursor);

        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(selected ? 12 : 14, 8, 12, 8);
        rowLayout->setSpacing(10);

        auto *check = new QCheckBox(row);
        check->setObjectName(QStringLiteral("devCheck_") + info.serial);
        check->setChecked(checked);
        check->setToolTip(tr("Include this device in bulk actions"));
        connect(check, &QCheckBox::toggled, this, [this, uim, serial = info.serial](bool on) {
            if (on)
                uim->m_checkedDevices.insert(serial);
            else
                uim->m_checkedDevices.remove(serial);
            refreshCheckedDevicesList();
        });
        rowLayout->addWidget(check);

        auto *statusDot = new QLabel(info.online ? QStringLiteral("●") : QStringLiteral("○"), row);
        statusDot->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                     .arg(info.online ? colGreen : colMuted));
        statusDot->setFixedWidth(12);
        rowLayout->addWidget(statusDot);

        auto *name = new QLabel(info.name, row);
        name->setStyleSheet(QStringLiteral("color: %1; background: transparent; font-weight: %2;")
                                .arg(colText, selected ? QStringLiteral("600")
                                                       : QStringLiteral("normal")));
        name->setToolTip(info.serial);
        rowLayout->addWidget(name, 1);

        // Register the row so the shared event filter can route clicks to it.
        uim->m_deviceRowMap[row] = info;
        row->installEventFilter(uim->m_mainWindow);
        return row;
    };

    // ── Clear current list ────────────────────────────────────────────────────
    QVBoxLayout *devListVLayout = uim->m_ui->devListVLayout;
    while (devListVLayout->count() > 0) {
        QLayoutItem *item = devListVLayout->takeAt(0);
        if (QWidget *cw = item->widget()) cw->deleteLater();
        delete item;
    }

    // Remember previously selected device serial so we can restore it.
    const QString prevSerial = uim->m_selectedDeviceRow
        ? uim->m_deviceRowMap.value(uim->m_selectedDeviceRow).serial
        : QString();
    uim->m_deviceRowMap.clear();
    uim->m_selectedDeviceRow = nullptr;

    // ── Populate device list ──────────────────────────────────────────────────
    DevicesManager &dm = DevicesManager::instance();
    const QList<AdbDevice> connected = dm.connectedDevices();

    QWidget *firstRow     = nullptr;
    QWidget *restoredRow  = nullptr;

    if (connected.isEmpty()) {
        QLabel *placeholder = new QLabel(tr("No devices connected.\nConnect a device via USB or WiFi."));
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(QString("color: %1;").arg(colMuted));
        devListVLayout->addWidget(placeholder);
        devListVLayout->addStretch();
        updateDeviceDetails({});
        return;
    }

    // Remove checked serials that are no longer connected
    QSet<QString> connectedIds;
    for (const AdbDevice &d : connected)
        connectedIds.insert(d.id);
    uim->m_checkedDevices.intersect(connectedIds);

    for (const AdbDevice &d : connected) {
        const UiManager::DeviceInfo info { d.id, d.name, QString(), d.isOnline };
        const bool selected = (!prevSerial.isEmpty() && d.id == prevSerial);
        const bool checked  = uim->m_checkedDevices.contains(d.id);
        QWidget *row = makeDeviceRow(info, selected, checked);
        devListVLayout->addWidget(row);
        if (!firstRow)   firstRow   = row;
        if (!prevSerial.isEmpty() && d.id == prevSerial)
            restoredRow = row;
    }

    devListVLayout->addStretch();

    // ── Restore / initial selection ───────────────────────────────────────────
    if (restoredRow) {
        uim->m_selectedDeviceRow = restoredRow;
        updateDeviceDetails(uim->m_deviceRowMap[restoredRow]);
    } else {
        // No previously selected device — show default empty state
        updateDeviceDetails({});
    }

    // ── Update the checked devices list in the right panel ───────────────────
    refreshCheckedDevicesList();
}

void DevicesTabController::onDevicesOrGroupsChanged()
{
    refreshDevicesTab();
}

void DevicesTabController::selectDeviceRow(QWidget *row, const UiManager::DeviceInfo &info)
{
    UiManager *uim = m_owner;
    const ColorScheme &cs = ColorScheme::instance();
    const QString colAccent = ColorScheme::toHex(cs.accent());
    const QString colText   = ColorScheme::toHex(cs.text());
    const QString colRowSel = ColorScheme::toHex(cs.rowSelectedBackground());

    // Deselect previous
    if (uim->m_selectedDeviceRow && uim->m_selectedDeviceRow != row) {
        const QString prevName = uim->m_selectedDeviceRow->objectName();
        uim->m_selectedDeviceRow->setStyleSheet(
            QString("QWidget#%1 { background-color: transparent; border-left: 2px solid transparent; }")
            .arg(prevName));
        if (auto *hl = qobject_cast<QHBoxLayout*>(uim->m_selectedDeviceRow->layout())) {
            for (int i = 0; i < hl->count(); ++i) {
                if (auto *nameW = qobject_cast<QWidget*>(hl->itemAt(i)->widget())) {
                    if (nameW->layout()) {
                        if (auto *lbl = qobject_cast<QLabel*>(nameW->layout()->itemAt(0)->widget()))
                            lbl->setStyleSheet(QString("color: %1; font-weight: normal;").arg(colText));
                    }
                }
            }
        }
    }

    // Select new row
    uim->m_selectedDeviceRow = row;
    const QString rowName = row->objectName();
    row->setStyleSheet(
        QString("QWidget#%1 { background-color: %3; border-left: 2px solid %2; }")
        .arg(rowName, colAccent, colRowSel));
    row->layout()->setContentsMargins(12, 8, 12, 8);

    updateDeviceDetails(info);
}

void DevicesTabController::refreshCheckedDevicesList()
{
    UiManager *uim = m_owner;
    const ColorScheme &cs = ColorScheme::instance();
    const QString colText   = ColorScheme::toHex(cs.text());
    const QString colMuted  = ColorScheme::toHex(cs.mutedText());
    const QString colGreen  = ColorScheme::toHex(cs.success());
    const QString colBorder = ColorScheme::toHex(cs.border());
    const QString colAccent = ColorScheme::toHex(cs.accent());

    QVBoxLayout *listLayout = uim->m_ui->devDeviceListVLayout;
    while (listLayout->count() > 0) {
        QLayoutItem *item = listLayout->takeAt(0);
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }

    if (uim->m_checkedDevices.isEmpty()) {
        QLabel *empty = new QLabel(tr("No devices selected.\nTick checkboxes in the sidebar to select devices."));
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        empty->setStyleSheet(
            QString("color: %1; font-style: italic;"
                    " background: transparent; border: none;").arg(colMuted));
        listLayout->addWidget(empty);

        // Disable all action buttons when no devices are checked
        for (QPushButton *btn : {uim->m_ui->devBtnReboot,
                                  uim->m_ui->devBtnVolumeUp, uim->m_ui->devBtnRebootBootloader,
                                  uim->m_ui->devBtnVolumeDown,
                                  uim->m_ui->devBtnRebootSideload, uim->m_ui->devBtnAdbWireless,
                                  uim->m_ui->devBtnAdbRoot, uim->m_ui->devBtnAdbUnroot,
                                  uim->m_ui->devBtnRebootFastboot,
                                  uim->m_ui->devBtnPowerKey,
                                  uim->m_ui->devBtnConnectWifi,
                                  uim->m_ui->devBtnFlash,
                                  uim->m_ui->devBtnDeployConfig})
            btn->setEnabled(false);

        // ── Clear all device-specific labels (header, stats, system info) ───
        const QString empty2 = tr("--");
        uim->m_ui->devNameLabel->setText(tr("No device selected"));
        uim->m_ui->devBatteryValue->setText(empty2);
        uim->m_ui->devIpValue->setText(empty2);
        uim->m_ui->devNetworkValue->setText(empty2);
        for (QLabel *lbl : {uim->m_ui->devSiManufacturerValue,
                            uim->m_ui->devSiModelValue,
                            uim->m_ui->devSiAndroidVersionValue,
                            uim->m_ui->devSiSdkVersionValue,
                            uim->m_ui->devSiBuildNumberValue,
                            uim->m_ui->devSiBuildFingerprintValue,
                            uim->m_ui->devSiSecurityPatchValue,
                            uim->m_ui->devSiKernelVersionValue,
                            uim->m_ui->devSiAbiValue})
            lbl->setText(empty2);

        // Drop any lingering selection so future clicks re-trigger fetch
        uim->m_selectedDeviceRow = nullptr;
        return;
    }

    DevicesManager &dm = DevicesManager::instance();
    const QList<AdbDevice> connected = dm.connectedDevices();
    QMap<QString, AdbDevice> deviceById;
    for (const AdbDevice &d : connected)
        deviceById.insert(d.id, d);

    for (const QString &id : uim->m_checkedDevices) {
        const bool online = deviceById.contains(id) && deviceById[id].isOnline;
        const QString name = deviceById.contains(id) ? deviceById[id].name : id;
        const QString statusColor = online ? colGreen : colMuted;
        const QString dotChar     = online ? QStringLiteral("●") : QStringLiteral("○");

        QWidget *rowW = new QWidget();
        rowW->setObjectName(QStringLiteral("devSelectedRow_") + id);
        rowW->setStyleSheet(
            QString("background: transparent; border-bottom: 1px solid %1;").arg(colBorder));
        rowW->setCursor(Qt::PointingHandCursor);
        QHBoxLayout *rl = new QHBoxLayout(rowW);
        rl->setContentsMargins(0, 8, 0, 8);
        rl->setSpacing(10);

        QLabel *dot = new QLabel(dotChar);
        dot->setStyleSheet(QString("color: %1; background: transparent; border: none;").arg(statusColor));
        dot->setFixedWidth(14);

        QLabel *nameLbl = new QLabel(name);
        nameLbl->setStyleSheet(
            QString("color: %1; background: transparent; border: none;").arg(colText));

        QLabel *statusLbl = new QLabel(online ? tr("Online") : tr("Offline"));
        statusLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusLbl->setStyleSheet(
            QString("color: %1; background: transparent; border: none;").arg(statusColor));
        statusLbl->setFixedWidth(50);

        rl->addWidget(dot);
        rl->addWidget(nameLbl, 1);
        rl->addWidget(statusLbl);

        // Click on this row to show device details
        rowW->installEventFilter(uim->m_mainWindow);
        listLayout->addWidget(rowW);
    }

    // Enable/disable all action buttons based on checked devices
    const bool hasChecked = !uim->m_checkedDevices.isEmpty();
    for (QPushButton *btn : {uim->m_ui->devBtnReboot,
                              uim->m_ui->devBtnVolumeUp, uim->m_ui->devBtnRebootBootloader,
                              uim->m_ui->devBtnVolumeDown,
                              uim->m_ui->devBtnRebootSideload, uim->m_ui->devBtnAdbWireless,
                              uim->m_ui->devBtnAdbRoot, uim->m_ui->devBtnAdbUnroot,
                              uim->m_ui->devBtnRebootFastboot,
                              uim->m_ui->devBtnPowerKey,
                              uim->m_ui->devBtnConnectWifi,
                              uim->m_ui->devBtnFlash,
                              uim->m_ui->devBtnDeployConfig})
        btn->setEnabled(hasChecked);

    // ── Auto-update detail panel when exactly one device is selected ─────────
    if (uim->m_checkedDevices.size() == 1) {
        const QString id = *uim->m_checkedDevices.begin();
        UiManager::DeviceInfo info;
        info.serial = id;
        if (deviceById.contains(id)) {
            info.name   = deviceById[id].name;
            info.online = deviceById[id].isOnline;
        } else {
            info.name   = id;
            info.online = false;
        }
        updateDeviceDetails(info);
    } else if (uim->m_checkedDevices.size() > 1) {
        // Multiple devices: show generic placeholder so stale single-device
        // values don't linger.
        const QString placeholder = tr("--");
        uim->m_ui->devNameLabel->setText(
            tr("%1 devices selected").arg(uim->m_checkedDevices.size()));
        uim->m_ui->devBatteryValue->setText(placeholder);
        uim->m_ui->devIpValue->setText(placeholder);
        uim->m_ui->devNetworkValue->setText(placeholder);
        for (QLabel *lbl : {uim->m_ui->devSiManufacturerValue,
                            uim->m_ui->devSiModelValue,
                            uim->m_ui->devSiAndroidVersionValue,
                            uim->m_ui->devSiSdkVersionValue,
                            uim->m_ui->devSiBuildNumberValue,
                            uim->m_ui->devSiBuildFingerprintValue,
                            uim->m_ui->devSiSecurityPatchValue,
                            uim->m_ui->devSiKernelVersionValue,
                            uim->m_ui->devSiAbiValue})
            lbl->setText(placeholder);
        uim->m_selectedDeviceRow = nullptr;
    }
}

void DevicesTabController::updateDeviceDetails(const UiManager::DeviceInfo &info)
{
    UiManager *uim = m_owner;
    const bool online = info.online;
    const bool hasDevice = !info.serial.isEmpty();

    // ── Header bar ──────────────────────────────────────────────────────────
    uim->m_ui->devNameLabel->setText(hasDevice ? info.name : tr("Select a device"));

    // ── Dashboard info ────────────────────────────────────────────────────────
    uim->m_ui->devBatteryValue->setText(online ? tr("...") : tr("--"));
    uim->m_ui->devIpValue->setText(online ? tr("...") : tr("--"));
    uim->m_ui->devNetworkValue->setText(online ? tr("...") : tr("--"));

    // Kick off async fetch for battery + IP
    if (online && !info.serial.isEmpty())
        DevicesManager::instance().fetchDeviceDetails(info.serial);

    // ── Action buttons: disable when offline OR no checked devices ────────────
    const bool canAct = online && !uim->m_checkedDevices.isEmpty();
    for (QPushButton *btn : {uim->m_ui->devBtnReboot,
                              uim->m_ui->devBtnVolumeUp, uim->m_ui->devBtnRebootBootloader,
                              uim->m_ui->devBtnVolumeDown,
                              uim->m_ui->devBtnRebootSideload, uim->m_ui->devBtnAdbWireless,
                              uim->m_ui->devBtnAdbRoot, uim->m_ui->devBtnAdbUnroot,
                              uim->m_ui->devBtnRebootFastboot,
                              uim->m_ui->devBtnPowerKey,
                              uim->m_ui->devBtnConnectWifi,
                              uim->m_ui->devBtnFlash,
                              uim->m_ui->devBtnDeployConfig
                            })
        btn->setEnabled(canAct);

    // ── System information: reset all values ────────────────────────────────
    const QString placeholder = online ? tr("...") : tr("--");
    uim->m_ui->devSiManufacturerValue->setText(placeholder);
    uim->m_ui->devSiModelValue->setText(placeholder);
    uim->m_ui->devSiAndroidVersionValue->setText(placeholder);
    uim->m_ui->devSiSdkVersionValue->setText(placeholder);
    uim->m_ui->devSiBuildNumberValue->setText(placeholder);
    uim->m_ui->devSiBuildFingerprintValue->setText(placeholder);
    uim->m_ui->devSiSecurityPatchValue->setText(placeholder);
    uim->m_ui->devSiKernelVersionValue->setText(placeholder);
    uim->m_ui->devSiAbiValue->setText(placeholder);
}

void DevicesTabController::onDeviceDetailsFetched(const DeviceDetails &details)
{
    UiManager *uim = m_owner;
    // Accept the result if it matches either:
    //   (a) the explicitly clicked sidebar row, or
    //   (b) the single auto-selected device (when exactly 1 is checked).
    bool matches = false;
    if (uim->m_selectedDeviceRow) {
        const UiManager::DeviceInfo &sel = uim->m_deviceRowMap.value(uim->m_selectedDeviceRow);
        if (sel.serial == details.serial) matches = true;
    }
    if (!matches && uim->m_checkedDevices.size() == 1
        && *uim->m_checkedDevices.begin() == details.serial) {
        matches = true;
    }
    if (!matches) return;

    uim->m_ui->devBatteryValue->setText(details.batteryLevel);
    uim->m_ui->devIpValue->setText(details.ipAddress);
    uim->m_ui->devNetworkValue->setText(details.networkStatus);

    // ── System information: update static labels ─────────────────────────────
    uim->m_ui->devSiManufacturerValue->setText(details.manufacturer);
    uim->m_ui->devSiModelValue->setText(details.model);
    uim->m_ui->devSiAndroidVersionValue->setText(details.androidVersion);
    uim->m_ui->devSiSdkVersionValue->setText(details.sdkVersion);
    uim->m_ui->devSiBuildNumberValue->setText(details.buildNumber);
    uim->m_ui->devSiBuildFingerprintValue->setText(details.buildFingerprint);
    uim->m_ui->devSiSecurityPatchValue->setText(details.securityPatch);
    uim->m_ui->devSiKernelVersionValue->setText(details.kernelVersion);
    uim->m_ui->devSiAbiValue->setText(details.abi);
}


