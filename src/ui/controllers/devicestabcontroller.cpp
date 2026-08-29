#include "devicestabcontroller.h"
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "devicesmanager.h"
#include "adbcommand.h"
#include "colorscheme.h"
#include "components/components.h"

#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTabBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

#include <memory>

namespace {
/** Firmware-flash progress dialog geometry. */
constexpr int kFlashDialogWidth  = 720;
constexpr int kFlashDialogHeight = 520;
} // namespace

DevicesTabController::DevicesTabController(UiManager *owner, QObject *parent)
    : QObject(parent), m_owner(owner) {}

QStringList DevicesTabController::selectedSerials() const
{
    UiManager *uim = m_owner;
    if (!uim->m_checkedDevices.isEmpty()) {
        QStringList serials;
        const QList<AdbDevice> connected = DevicesManager::instance().connectedDevices();
        QSet<QString> onlineIds;
        for (const AdbDevice &d : connected)
            if (d.isOnline) onlineIds.insert(d.id);
        for (const QString &id : uim->m_checkedDevices)
            if (onlineIds.contains(id)) serials.append(id);
        return serials;
    }
    if (!uim->m_selectedDeviceRow) return {};
    const QString s = uim->m_deviceRowMap.value(uim->m_selectedDeviceRow).serial;
    return s.isEmpty() ? QStringList{} : QStringList{s};
}

void DevicesTabController::runOnSelected(QStringList (*adbCommandFactory)(const QString &))
{
    DevicesManager &mgr = DevicesManager::instance();
    for (const QString &s : selectedSerials())
        mgr.runAdbCommand(adbCommandFactory(s));
}
namespace {

/** Shell used to run the vendor flash script, per platform. */
QString flashShell()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("cmd.exe");
#else
    return QStringLiteral("/bin/bash");
#endif
}

QStringList flashShellArgs(const QString &command)
{
#if defined(Q_OS_WIN)
    return {QStringLiteral("/c"), command};
#else
    return {QStringLiteral("-c"), command};
#endif
}

/** The flash command run for one device, from the firmware directory. */
QString flashCommand(const QString &serial)
{
#if defined(Q_OS_WIN)
    return QStringLiteral("set ANDROID_SERIAL=%1 && adb -s %1 reboot bootloader && download.sh")
        .arg(serial);
#else
    return QStringLiteral("export ANDROID_SERIAL=%1 ; adb -s %1 reboot bootloader && ./download.sh")
        .arg(serial);
#endif
}

} // namespace

void DevicesTabController::runFlashFirmware()
{
    UiManager *uim = m_owner;

    const QStringList serials = selectedSerials();
    if (serials.isEmpty()) {
        QMessageBox::warning(uim->m_mainWindow, tr("No Device"),
                             tr("Please select a device first."));
        return;
    }

    const QString firmwarePath = uim->m_ui->devFirmwarePathEdit->text().trimmed();
    if (firmwarePath.isEmpty()) {
        QMessageBox::warning(uim->m_mainWindow, tr("No Firmware Path"),
                             tr("Please select a firmware directory first."));
        return;
    }

    if (!QFileInfo::exists(firmwarePath + QStringLiteral("/download.sh"))) {
        QMessageBox::warning(uim->m_mainWindow, tr("Script Not Found"),
                             tr("download.sh not found in the firmware directory."));
        return;
    }

    // ── Progress dialog ──────────────────────────────────────────────────────
    auto *dialog = new QDialog(uim->m_mainWindow);
    dialog->setWindowTitle(tr("Flash Firmware"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(kFlashDialogWidth, kFlashDialogHeight);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *statusLabel = UiComponents::Label::h3(
        tr("Flashing %n device(s)…", nullptr, serials.size()), dialog);
    layout->addWidget(statusLabel);

    auto *outputView = new QTextEdit(dialog);
    outputView->setObjectName(QStringLiteral("flashOutputView"));
    outputView->setReadOnly(true);
    outputView->setProperty("role", QStringLiteral("mono"));
    layout->addWidget(outputView, 1);

    auto *closeButton = UiComponents::Button::make(tr("Close"),
                                                   UiComponents::ButtonVariant::Primary,
                                                   dialog);
    closeButton->setEnabled(false);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::accept);

    dialog->show();

    // Progress shared by every device's process. Owned by the dialog, so it
    // cannot outlive the widgets the callbacks touch — the previous version
    // used raw new'd counters that both the finished and errorOccurred
    // handlers deleted, which double-freed whenever a process failed to start.
    struct FlashProgress {
        int  remaining = 0;
        bool anyFailed = false;
    };
    auto progress = std::make_shared<FlashProgress>();
    progress->remaining = serials.size();

    auto reportDeviceDone = [progress, statusLabel, closeButton](bool ok) {
        if (!ok)
            progress->anyFailed = true;
        if (--progress->remaining > 0)
            return;

        const bool allOk = !progress->anyFailed;
        statusLabel->setText(allOk ? tr("All devices flashed successfully.")
                                   : tr("Flashing completed with errors on some devices."));
        const ColorScheme &colors = ColorScheme::instance();
        statusLabel->setStyleSheet(
            QStringLiteral("color: %1; font-weight: 600;")
                .arg(ColorScheme::toHex(allOk ? colors.success() : colors.danger())));
        closeButton->setEnabled(true);
    };

    // ── One process per device, run in parallel ──────────────────────────────
    for (const QString &serial : serials) {
        const QString command = flashCommand(serial);
        outputView->append(QStringLiteral("[%1] $ %2\n").arg(serial, command));

        auto *process = new QProcess(dialog);
        process->setWorkingDirectory(firmwarePath);
        process->setProcessChannelMode(QProcess::MergedChannels);

        connect(process, &QProcess::readyRead, dialog, [process, outputView, serial]() {
            const QString text = QString::fromUtf8(process->readAll());
            for (const QString &line : text.split(QLatin1Char('\n'))) {
                if (line.isEmpty())
                    continue;
                outputView->moveCursor(QTextCursor::End);
                outputView->insertPlainText(QStringLiteral("[%1] %2\n").arg(serial, line));
            }
            outputView->moveCursor(QTextCursor::End);
        });

        connect(process, &QProcess::finished, dialog,
                [serial, outputView, reportDeviceDone](int exitCode, QProcess::ExitStatus status) {
            outputView->append(tr("[%1] --- Finished (exit code: %2) ---\n")
                                   .arg(serial).arg(exitCode));
            reportDeviceDone(status == QProcess::NormalExit && exitCode == 0);
        });

        // A process that never starts emits errorOccurred but not finished, so
        // it has to report completion on its own — exactly once.
        connect(process, &QProcess::errorOccurred, dialog,
                [serial, outputView, reportDeviceDone](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
                return;   // other errors are still followed by finished()
            outputView->append(tr("[%1] --- Failed to start command ---\n").arg(serial));
            reportDeviceDone(false);
        });

        process->start(flashShell(), flashShellArgs(command));
    }
}

void DevicesTabController::setup()
{
    UiManager *uim = m_owner;
    // Connect DevicesManager -> refresh the sidebar whenever anything changes.
    connect(&DevicesManager::instance(), &DevicesManager::devicesOrGroupsChanged,
            this, &DevicesTabController::onDevicesOrGroupsChanged, Qt::UniqueConnection);

    // Connect DevicesManager -> update dashboard when device details arrive.
    connect(&DevicesManager::instance(), &DevicesManager::deviceDetailsFetched,
            this, &DevicesTabController::onDeviceDetailsFetched, Qt::UniqueConnection);

    // Hide the "New Group" button (no longer used).
    uim->m_ui->devBtnAddGroup->hide();

    // ── Quick-action button connections ──────────────────────────────────────
    auto selectedSerials = [this] { return this->selectedSerials(); };

    // Reboot device
    connect(uim->m_ui->devBtnReboot, &QPushButton::clicked, this,
            [this]{ runOnSelected(&AdbCommand::rebootDevice); });

    // Reboot sideload
    connect(uim->m_ui->devBtnRebootSideload, &QPushButton::clicked, this,
            [this]{ runOnSelected(&AdbCommand::rebootSideload); });

    // Reboot bootloader
    connect(uim->m_ui->devBtnRebootBootloader, &QPushButton::clicked, this,
            [this]{ runOnSelected(&AdbCommand::rebootBootloader); });

    // Volume up
    connect(uim->m_ui->devBtnVolumeUp, &QPushButton::clicked, this,
            [this]{ runOnSelected(&AdbCommand::volumeUp); });

    // Volume down
    connect(uim->m_ui->devBtnVolumeDown, &QPushButton::clicked, this,
            [this]{ runOnSelected(&AdbCommand::volumeDown); });

    // Connect WiFi (uses static input fields in devInfoGrid)
    {
        auto &mgr = DevicesManager::instance();
        uim->m_ui->devWifiSsidEdit->setText(mgr.savedWifiSsid());
        uim->m_ui->devWifiPassEdit->setText(mgr.savedWifiPassword());
    }
    connect(uim->m_ui->devBtnConnectWifi, &QPushButton::clicked, uim, [uim, selectedSerials]() {
        const QStringList serials = selectedSerials();
        if (serials.isEmpty()) return;

        const QString ssid = uim->m_ui->devWifiSsidEdit->text().trimmed();
        const QString pass = uim->m_ui->devWifiPassEdit->text();
        if (ssid.isEmpty()) return;

        auto &mgr = DevicesManager::instance();
        mgr.saveWifiCredentials(ssid, pass);
        for (const QString &serial : serials)
            mgr.runAdbCommand(AdbCommand::connectWifi(serial, ssid, pass));
    });

    // Browse firmware path
    connect(uim->m_ui->devBtnBrowseFirmware, &QPushButton::clicked, uim, [uim]() {
        const QString dir = QFileDialog::getExistingDirectory(
            uim->m_mainWindow, tr("Select Firmware Directory"),
            uim->m_ui->devFirmwarePathEdit->text().isEmpty()
                ? QDir::homePath()
                : uim->m_ui->devFirmwarePathEdit->text());
        if (!dir.isEmpty())
            uim->m_ui->devFirmwarePathEdit->setText(dir);
    });

    // All action buttons start disabled (enabled when devices are checked)
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
        btn->setEnabled(!uim->m_checkedDevices.isEmpty());

    // Flash firmware: reboot bootloader + run download.sh with progress dialog
    connect(uim->m_ui->devBtnFlash, &QPushButton::clicked, this,
            [this]{ runFlashFirmware(); });

    // ADB Wireless connect
    connect(uim->m_ui->devBtnAdbWireless, &QPushButton::clicked, this, [this]{
        for (const QString &s : this->selectedSerials())
            DevicesManager::instance().enableAdbWireless(s);
    });

    // ADB root
    connect(uim->m_ui->devBtnAdbRoot, &QPushButton::clicked, this,
            [this]{ runOnSelected(&AdbCommand::root); });

    // ADB unroot
    connect(uim->m_ui->devBtnAdbUnroot, &QPushButton::clicked, this,
            [this]{ runOnSelected(&AdbCommand::unroot); });

    // Reboot fastboot
    connect(uim->m_ui->devBtnRebootFastboot, &QPushButton::clicked, this,
            [this]{ runOnSelected(&AdbCommand::rebootFastboot); });

    // Power key input
    connect(uim->m_ui->devBtnPowerKey, &QPushButton::clicked, this,
            [this]{ runOnSelected(&AdbCommand::powerKey); });

    setupConfigTab();
    polishDashboardCards();
    refreshDevicesTab();

    // Fix tab-bar header truncation in the two inner tab widgets.
    //
    // Two issues were causing the labels ("Dashboard", "Device Config",
    // "App Installer", "Device Settings", "Display") to clip:
    //   1. Default elide mode (Qt::ElideRight) trims long labels with "…".
    //   2. The stylesheet sets `font-weight: 600` on `QTabBar::tab`, but Qt
    //      computes tab width using the bar's regular font — then paints
    //      bold glyphs which are wider than the measured width and get
    //      clipped on the right.
    // Disable elide, allow scroll buttons on overflow, and set the bold
    // weight on the QFont so size hints match the painted text.
    auto configureInnerTabBar = [](QTabWidget *tw) {
        if (!tw) return;
        if (QTabBar *bar = tw->tabBar()) {
            bar->setElideMode(Qt::ElideNone);
            bar->setExpanding(false);
            bar->setUsesScrollButtons(true);
            QFont f = bar->font();
            f.setWeight(QFont::DemiBold);  // matches CSS font-weight: 600
            bar->setFont(f);
        }
    };
    configureInnerTabBar(uim->m_ui->devInnerTabWidget);
    configureInnerTabBar(uim->m_ui->devConfigTabWidget);
}

// ─────────────────────────────────────────────────────────────────────────────
// polishDashboardCards: wraps the dashboard sections (sysinfo, stats, wifi,
// firmware, quick actions, configuration) in styled QFrame cards. Also splits
// the cluttered devInfoGrid into separate Stats / WiFi / Firmware groupings.
// ─────────────────────────────────────────────────────────────────────────────

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <QSpacerItem>

namespace {

// Wrap an existing layout (already inside `host` at index `idx`) inside a new
// styled QFrame card. Returns the newly inserted card.
QFrame *wrapLayoutInCard(QBoxLayout *host, int idx, const QString &cardName,
                         int padH = 18, int padV = 16)
{
    QLayoutItem *item = host->itemAt(idx);
    if (!item) return nullptr;
    QLayout *inner = item->layout();
    if (!inner) return nullptr;

    auto *card = new QFrame(host->parentWidget());
    card->setObjectName(cardName);
    card->setProperty("class", "devCard");
    card->setFrameShape(QFrame::NoFrame);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(padH, padV, padH, padV);
    cardLayout->setSpacing(10);

    // Detach inner layout from host (takeAt removes & returns ownership of
    // the QLayoutItem). For a layout-item, the item *is* the layout, so we
    // must NOT delete it — addLayout() reparents it to cardLayout.
    QLayoutItem *taken = host->takeAt(idx);
    Q_UNUSED(taken);
    cardLayout->addLayout(inner);

    host->insertWidget(idx, card);
    return card;
}

} // namespace

void DevicesTabController::polishDashboardCards()
{
    auto *uim = m_owner;
    auto *ui  = uim->m_ui;

    // ── Step 1: wrap each section currently inside devInfoHLayout & devBottomHLayout
    //
    // Current top-level structure of devDashVLayout:
    //   item 0: devInfoHLayout  (HBox: deviceListContainer, hSpacer, sysInfoVLayout, hSpacer, infoGrid, hSpacer)
    //   item 1: devBottomHLayout (HBox: qaVLayout, configVLayout)
    //
    // We'll rebuild devInfoHLayout to: [sysInfoCard] [statsCard / wifiCard / firmwareCard column]
    // and rebuild devBottomHLayout to:  [qaCard] [configCard]

    // ── Configuration card (bottom right) ──
    if (auto *bottom = ui->devBottomHLayout) {
        // bottom items: [0]=devQaVLayout, [1]=devConfigVLayout
        // wrap each
        // Note: indices shift after each wrap, so wrap from highest index first
        if (bottom->count() >= 2) {
            wrapLayoutInCard(bottom, 1, "devConfigCard");
            wrapLayoutInCard(bottom, 0, "devQaCard");
        }
        bottom->setSpacing(14);
        bottom->setStretch(0, 1);
        bottom->setStretch(1, 1);
    }

    // ── Top dashboard row: rebuild ──
    auto *info = ui->devInfoHLayout;
    if (info && info->count() >= 5) {
        // First, clean out the 3 horizontal spacers (indices 1, 3, 5 typically)
        // Walk backwards and remove any QSpacerItem with width hint > 0
        for (int i = info->count() - 1; i >= 0; --i) {
            QLayoutItem *it = info->itemAt(i);
            if (it && it->spacerItem()) {
                QLayoutItem *taken = info->takeAt(i);
                delete taken; // QSpacerItem owns nothing else — safe to delete.
            }
        }

        // Now items should be: [0]=deviceListContainer, [1]=sysInfoVLayout, [2]=infoGrid
        // Wrap sysInfo as a card (index 1) — wrap from highest first
        if (info->count() >= 3) {
            // ── Split the messy infoGrid into Stats + WiFi + Firmware cards ──
            // The grid has rows: 0=Battery, 1=IP, 2=Network, 3=SSID, 4=Pass, 5=ConnectBtn, 7=FirmwarePath, 8=FlashBtn
            // We'll detach the grid widgets and place them into 3 new layouts.
            QLayoutItem *gridItem = info->itemAt(2);
            QGridLayout *grid = gridItem ? qobject_cast<QGridLayout *>(gridItem->layout()) : nullptr;
            if (grid) {
                // Detach the grid layout from info (do NOT delete the item —
                // for a layout-item, the item *is* the layout).
                QLayoutItem *takenGrid = info->takeAt(2);
                Q_UNUSED(takenGrid);

                // Build a vertical stack of 3 cards: stats / wifi / firmware
                auto *rightCol = new QVBoxLayout();
                rightCol->setContentsMargins(0, 0, 0, 0);
                rightCol->setSpacing(14);

                // Helper: take a widget out of the grid (releases ownership to caller)
                auto take = [&](QWidget *w) {
                    if (!w) return;
                    grid->removeWidget(w);
                    w->setParent(nullptr);
                };

                // STATS CARD: 3 mini-stat columns (Battery / IP / Network)
                auto *statsCard = new QFrame();
                statsCard->setObjectName("devStatsCard");
                statsCard->setProperty("class", "devCard");
                statsCard->setFrameShape(QFrame::NoFrame);
                auto *statsLayout = new QHBoxLayout(statsCard);
                statsLayout->setContentsMargins(18, 16, 18, 16);
                statsLayout->setSpacing(20);

                auto addStat = [&](QWidget *icon, QWidget *title, QWidget *value) {
                    take(icon); take(title); take(value);
                    auto *col = new QVBoxLayout();
                    col->setSpacing(2);
                    auto *headRow = new QHBoxLayout();
                    headRow->setSpacing(6);
                    if (icon)  headRow->addWidget(icon);
                    if (title) headRow->addWidget(title);
                    headRow->addStretch();
                    col->addLayout(headRow);
                    if (value) col->addWidget(value);
                    statsLayout->addLayout(col, 1);
                };
                addStat(ui->devBatteryIcon, ui->devBatteryTitle, ui->devBatteryValue);
                addStat(ui->devIpIcon,      ui->devIpTitle,      ui->devIpValue);
                addStat(ui->devNetworkIcon, ui->devNetworkTitle, ui->devNetworkValue);

                rightCol->addWidget(statsCard);

                // WIFI CARD: SSID + password + Connect button
                auto *wifiCard = new QFrame();
                wifiCard->setObjectName("devWifiCard");
                wifiCard->setProperty("class", "devCard");
                wifiCard->setFrameShape(QFrame::NoFrame);
                auto *wifiLayout = new QGridLayout(wifiCard);
                wifiLayout->setContentsMargins(18, 16, 18, 16);
                wifiLayout->setHorizontalSpacing(12);
                wifiLayout->setVerticalSpacing(10);

                take(ui->devWifiSsidIcon);  take(ui->devWifiSsidTitle);  take(ui->devWifiSsidEdit);
                take(ui->devWifiPassIcon);  take(ui->devWifiPassTitle);  take(ui->devWifiPassEdit);
                take(ui->devBtnConnectWifi);

                wifiLayout->addWidget(ui->devWifiSsidIcon,  0, 0);
                wifiLayout->addWidget(ui->devWifiSsidTitle, 0, 1);
                wifiLayout->addWidget(ui->devWifiSsidEdit,  0, 2);
                wifiLayout->addWidget(ui->devWifiPassIcon,  1, 0);
                wifiLayout->addWidget(ui->devWifiPassTitle, 1, 1);
                wifiLayout->addWidget(ui->devWifiPassEdit,  1, 2);
                wifiLayout->addWidget(ui->devBtnConnectWifi,2, 2);
                wifiLayout->setColumnStretch(2, 1);

                rightCol->addWidget(wifiCard);

                // FIRMWARE CARD: path + browse + flash button
                auto *fwCard = new QFrame();
                fwCard->setObjectName("devFirmwareCard");
                fwCard->setProperty("class", "devCard");
                fwCard->setFrameShape(QFrame::NoFrame);
                auto *fwLayout = new QGridLayout(fwCard);
                fwLayout->setContentsMargins(18, 16, 18, 16);
                fwLayout->setHorizontalSpacing(12);
                fwLayout->setVerticalSpacing(10);

                take(ui->devFirmwareIcon);
                take(ui->devFirmwareTitle);
                take(ui->devFirmwarePathEdit);
                take(ui->devBtnBrowseFirmware);
                take(ui->devBtnFlash);

                fwLayout->addWidget(ui->devFirmwareIcon,     0, 0);
                fwLayout->addWidget(ui->devFirmwareTitle,    0, 1);
                fwLayout->addWidget(ui->devFirmwarePathEdit, 0, 2);
                fwLayout->addWidget(ui->devBtnBrowseFirmware,0, 3);
                fwLayout->addWidget(ui->devBtnFlash,         1, 2, 1, 2);
                fwLayout->setColumnStretch(2, 1);

                rightCol->addWidget(fwCard);
                rightCol->addStretch();

                info->addLayout(rightCol, 1);

                // The original grid layout is now empty and unparented; free it.
                delete grid;
            }

            // Wrap sysInfo (index 1) as a card
            wrapLayoutInCard(info, 1, "devSysInfoCard");
            // Wrap deviceList container (index 0) as a card
            wrapLayoutInCard(info, 0, "devDeviceListCard");
        }

        info->setSpacing(14);
        info->setStretch(0, 0);
        info->setStretch(1, 1);
        info->setStretch(2, 1);
    }

    // Polish outer dashboard margins
    if (auto *outer = qobject_cast<QVBoxLayout *>(ui->devDashContents->layout())) {
        outer->setContentsMargins(20, 18, 20, 20);
        outer->setSpacing(14);
    }

    // ── Remove the QScrollArea wrapper so the dashboard fits the tab height ──
    // The original .ui wraps devDashContents inside devDashScrollArea. We
    // detach the contents widget and place it directly into the tab page
    // layout, then delete the (now empty) scroll area.
    if (ui->devDashScrollArea && ui->devDashContents && ui->devPageDashLayout) {
        QWidget *contents = ui->devDashContents;
        QScrollArea *scroll = ui->devDashScrollArea;
        QVBoxLayout *pageLayout = ui->devPageDashLayout;

        // Detach contents from scroll area (takeWidget releases ownership).
        scroll->takeWidget();

        // Remove the scroll area from the page layout, then add contents.
        const int idx = pageLayout->indexOf(scroll);
        if (idx >= 0) {
            QLayoutItem *taken = pageLayout->takeAt(idx);
            delete taken;
        }
        scroll->deleteLater();

        contents->setParent(ui->devPageDashboard);
        pageLayout->addWidget(contents);
    }
}

