#include "devicestabcontroller.h"
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "devicesmanager.h"
#include "adbcommand.h"
#include "adbexecutor.h"
#include "adbmanager.h"
#include "jsonfileio.h"
#include "presetdialogs.h"
#include "toggleswitch.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFont>
#include <QtConcurrent/QtConcurrent>

void DevicesTabController::setupConfigTab()
{
    UiManager *uim = m_owner;

    // Default: Developer Options is normally already enabled on engineering
    // devices (since the user is using adb), so reflect that in the UI.
    if (uim->m_ui->devDevOptsToggle) {
        uim->m_ui->devDevOptsToggle->setChecked(true);
        uim->m_ui->devAdbWifiToggle->setChecked(true);
        uim->m_ui->devBluetoothToggle->setChecked(true);
        uim->m_ui->devWifiToggle->setChecked(true);
        uim->m_ui->devTimeFormatToggle->setChecked(true);
        uim->m_ui->devAllowMockToggle->setChecked(true);
        uim->m_ui->devStayAwakeToggle->setChecked(true);
    }

    // selectedSerials helper — returns checked online serials, or the single
    // selected serial as fallback.
    auto selectedSerials = [this] { return this->selectedSerials(); };


    // ── Configuration tab: toggle switches are defined in mainwindow.ui ─────
    // devStayAwakeToggle and devAllowMockToggle are ToggleSwitch widgets.

    // Shrink description labels (name ends with "Desc") for compactness.
    // Use stylesheet (font-size + color) so it overrides any inherited font.
    if (uim->m_ui->devConfigTabWidget) {
        const auto allLabels = uim->m_ui->devConfigTabWidget->findChildren<QLabel *>();
        for (QLabel *lbl : allLabels) {
            const QString name = lbl->objectName();
            if (name.endsWith(QStringLiteral("Desc"))) {
                lbl->setStyleSheet(QStringLiteral(
                    "font-size: 10px; font-weight: normal; color: rgba(255,255,255,150);"));
                lbl->setMinimumWidth(280);
            } else if (name.endsWith(QStringLiteral("Label"))) {
                lbl->setMinimumWidth(280);
            }
        }
    }

    // Helper: build configuration JSON from current toggle states
    auto buildConfigJson = [uim]() -> QJsonObject {
        QJsonObject root;
        QJsonObject deviceSettings;
        deviceSettings["stay_awake"] = uim->m_ui->devStayAwakeToggle->isChecked();
        deviceSettings["allow_mock_modem"] = uim->m_ui->devAllowMockToggle->isChecked();
        deviceSettings["verifier_verify_adb_installs"] = uim->m_ui->devVerifyAdbToggle->isChecked();
        deviceSettings["system_locale"] = uim->m_ui->devLocaleEdit->text().trimmed();
        deviceSettings["time_12_24"] = uim->m_ui->devTimeFormatToggle->isChecked(); // true = 24h
        deviceSettings["wifi"] = uim->m_ui->devWifiToggle->isChecked();
        deviceSettings["bluetooth"] = uim->m_ui->devBluetoothToggle->isChecked();
        deviceSettings["airplane_mode"] = uim->m_ui->devAirplaneToggle->isChecked();
        deviceSettings["auto_time"] = uim->m_ui->devAutoTimeToggle->isChecked();
        deviceSettings["nfc"] = uim->m_ui->devNfcToggle->isChecked();
        deviceSettings["battery_saver"] = uim->m_ui->devBatterySaverToggle->isChecked();
        deviceSettings["developer_options"] = uim->m_ui->devDevOptsToggle->isChecked();
        deviceSettings["adb_wifi"] = uim->m_ui->devAdbWifiToggle->isChecked();
        deviceSettings["hidden_api_allow"] = uim->m_ui->devHiddenApiToggle->isChecked();
        root["device_settings"] = deviceSettings;
        QJsonObject displaySettings;
        displaySettings["auto_rotate"] = uim->m_ui->devAutoRotateToggle->isChecked();
        displaySettings["show_touches"] = uim->m_ui->devShowTouchesToggle->isChecked();
        displaySettings["pointer_location"] = uim->m_ui->devPointerLocationToggle->isChecked();
        displaySettings["dark_mode"] = uim->m_ui->devDarkModeToggle->isChecked();
        displaySettings["force_rtl"] = uim->m_ui->devForceRtlToggle->isChecked();
        displaySettings["animations_enabled"] = uim->m_ui->devAnimationsToggle->isChecked();
        root["display"] = displaySettings;
        return root;
    };

    // Helper: apply configuration JSON to toggle switches
    auto applyConfigJson = [uim](const QJsonObject &root) {
        if (root.contains("device_settings")) {
            QJsonObject ds = root["device_settings"].toObject();
            if (ds.contains("stay_awake"))
                uim->m_ui->devStayAwakeToggle->setChecked(ds["stay_awake"].toBool());
            if (ds.contains("allow_mock_modem"))
                uim->m_ui->devAllowMockToggle->setChecked(ds["allow_mock_modem"].toBool());
            // Backward compat: old presets may have allow_mock_locations
            if (ds.contains("allow_mock_locations") && !ds.contains("allow_mock_modem"))
                uim->m_ui->devAllowMockToggle->setChecked(ds["allow_mock_locations"].toBool());
            if (ds.contains("verifier_verify_adb_installs"))
                uim->m_ui->devVerifyAdbToggle->setChecked(ds["verifier_verify_adb_installs"].toBool());
            if (ds.contains("system_locale"))
                uim->m_ui->devLocaleEdit->setText(ds["system_locale"].toString());
            if (ds.contains("time_12_24"))
                uim->m_ui->devTimeFormatToggle->setChecked(ds["time_12_24"].toBool());
            if (ds.contains("wifi"))
                uim->m_ui->devWifiToggle->setChecked(ds["wifi"].toBool());
            if (ds.contains("bluetooth"))
                uim->m_ui->devBluetoothToggle->setChecked(ds["bluetooth"].toBool());
            if (ds.contains("airplane_mode"))
                uim->m_ui->devAirplaneToggle->setChecked(ds["airplane_mode"].toBool());
            if (ds.contains("auto_time"))
                uim->m_ui->devAutoTimeToggle->setChecked(ds["auto_time"].toBool());
            if (ds.contains("nfc"))
                uim->m_ui->devNfcToggle->setChecked(ds["nfc"].toBool());
            if (ds.contains("battery_saver"))
                uim->m_ui->devBatterySaverToggle->setChecked(ds["battery_saver"].toBool());
            if (ds.contains("developer_options"))
                uim->m_ui->devDevOptsToggle->setChecked(ds["developer_options"].toBool());
            if (ds.contains("adb_wifi"))
                uim->m_ui->devAdbWifiToggle->setChecked(ds["adb_wifi"].toBool());
            if (ds.contains("hidden_api_allow"))
                uim->m_ui->devHiddenApiToggle->setChecked(ds["hidden_api_allow"].toBool());
        }
        if (root.contains("display")) {
            QJsonObject dp = root["display"].toObject();
            if (dp.contains("auto_rotate"))
                uim->m_ui->devAutoRotateToggle->setChecked(dp["auto_rotate"].toBool());
            if (dp.contains("show_touches"))
                uim->m_ui->devShowTouchesToggle->setChecked(dp["show_touches"].toBool());
            if (dp.contains("pointer_location"))
                uim->m_ui->devPointerLocationToggle->setChecked(dp["pointer_location"].toBool());
            if (dp.contains("dark_mode"))
                uim->m_ui->devDarkModeToggle->setChecked(dp["dark_mode"].toBool());
            if (dp.contains("force_rtl"))
                uim->m_ui->devForceRtlToggle->setChecked(dp["force_rtl"].toBool());
            if (dp.contains("animations_enabled"))
                uim->m_ui->devAnimationsToggle->setChecked(dp["animations_enabled"].toBool());
        }
    };

    // ── Auto-update JSON view when config changes ────────────────────────
    auto updateJsonView = [uim, buildConfigJson]() {
        QJsonDocument doc(buildConfigJson());
        uim->m_ui->devJsonView->setPlainText(
            QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    };

    // Connect all toggles
    connect(uim->m_ui->devStayAwakeToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devAllowMockToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devVerifyAdbToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devTimeFormatToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devWifiToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devBluetoothToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devAirplaneToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devAutoTimeToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devNfcToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devBatterySaverToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devDevOptsToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devAdbWifiToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devHiddenApiToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devAutoRotateToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devShowTouchesToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devPointerLocationToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devDarkModeToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devForceRtlToggle, &ToggleSwitch::toggled, uim, updateJsonView);
    connect(uim->m_ui->devAnimationsToggle, &ToggleSwitch::toggled, uim, updateJsonView);

    // Connect locale input on Enter and on text change
    connect(uim->m_ui->devLocaleEdit, &QLineEdit::returnPressed, uim, updateJsonView);
    connect(uim->m_ui->devLocaleEdit, &QLineEdit::textChanged, uim, updateJsonView);

    // Generate initial JSON view
    updateJsonView();

    // ── Export JSON button ──────────────────────────────────────────────────
    connect(uim->m_ui->devBtnExportJson, &QPushButton::clicked, uim, [uim, buildConfigJson]() {
        const QString filePath = QFileDialog::getSaveFileName(
            uim->m_mainWindow, tr("Export Device Configuration"),
            QDir::homePath() + "/device_config.json",
            tr("JSON Files (*.json)"));
        if (filePath.isEmpty()) return;

        QString errorMsg;
        if (!JsonFileIo::writeFile(filePath, QJsonDocument(buildConfigJson()), errorMsg)) {
            QMessageBox::warning(uim->m_mainWindow, tr("Export Failed"),
                                 tr("Could not write to file:\n%1").arg(errorMsg));
            return;
        }
        QMessageBox::information(uim->m_mainWindow, tr("Export Successful"),
                                 tr("Configuration exported to:\n%1").arg(filePath));
    });

    // ── Import JSON button ──────────────────────────────────────────────────
    connect(uim->m_ui->devBtnImportJson, &QPushButton::clicked, uim, [uim, applyConfigJson]() {
        const QString filePath = QFileDialog::getOpenFileName(
            uim->m_mainWindow, tr("Import Device Configuration"),
            QDir::homePath(),
            tr("JSON Files (*.json)"));
        if (filePath.isEmpty()) return;

        QJsonDocument doc;
        QString errorMsg;
        if (!JsonFileIo::readFile(filePath, doc, errorMsg)) {
            QMessageBox::warning(uim->m_mainWindow, tr("Import Failed"),
                                 tr("Could not open file:\n%1").arg(errorMsg));
            return;
        }

        applyConfigJson(doc.object());
        uim->m_ui->devJsonView->setPlainText(
            QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));

        QMessageBox::information(uim->m_mainWindow, tr("Import Successful"),
                                 tr("Configuration loaded from:\n%1").arg(filePath));
    });

    // ── Deploy to Device button ─────────────────────────────────────────────
    connect(uim->m_ui->devBtnDeployConfig, &QPushButton::clicked, uim, [uim, selectedSerials, buildConfigJson]() {
        const QStringList serials = selectedSerials();
        if (serials.isEmpty()) {
            QMessageBox::warning(uim->m_mainWindow, tr("No Device"),
                                 tr("Please select a device or group first."));
            return;
        }

        // Read config from JSON view if available, otherwise from toggles
        QJsonObject config;
        const QString jsonText = uim->m_ui->devJsonView->toPlainText().trimmed();
        if (!jsonText.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8());
            if (!doc.isNull() && doc.isObject())
                config = doc.object();
        }
        if (config.isEmpty())
            config = buildConfigJson();

        QJsonObject ds = config["device_settings"].toObject();

        QVector<QPair<QString, QStringList>> commands;
        auto appendCommand = [&commands](const QString &serial, const QStringList &args) {
            commands.append({serial, args});
        };

        for (const QString &serial : serials) {
            if (ds.contains("stay_awake"))
                appendCommand(serial, AdbCommand::stayAwake(serial, ds["stay_awake"].toBool()));
            if (ds.contains("allow_mock_modem"))
                appendCommand(serial, AdbCommand::setMockModem(serial, ds["allow_mock_modem"].toBool()));
            if (ds.contains("verifier_verify_adb_installs"))
                appendCommand(serial, AdbCommand::setVerifyAdbInstalls(serial, ds["verifier_verify_adb_installs"].toBool()));
            if (ds.contains("system_locale") && !ds["system_locale"].toString().isEmpty())
                appendCommand(serial, AdbCommand::setSystemLocale(serial, ds["system_locale"].toString()));
            if (ds.contains("time_12_24"))
                appendCommand(serial, AdbCommand::setTimeFormat(serial, ds["time_12_24"].toBool()));
            if (ds.contains("wifi"))
                appendCommand(serial, AdbCommand::setWifiEnabled(serial, ds["wifi"].toBool()));
            if (ds.contains("bluetooth"))
                appendCommand(serial, AdbCommand::setBluetoothEnabled(serial, ds["bluetooth"].toBool()));
            if (ds.contains("airplane_mode"))
                appendCommand(serial, AdbCommand::setAirplaneMode(serial, ds["airplane_mode"].toBool()));
            if (ds.contains("auto_time"))
                appendCommand(serial, AdbCommand::setAutoTime(serial, ds["auto_time"].toBool()));
            if (ds.contains("nfc"))
                appendCommand(serial, AdbCommand::setNfcEnabled(serial, ds["nfc"].toBool()));
            if (ds.contains("battery_saver"))
                appendCommand(serial, AdbCommand::setBatterySaver(serial, ds["battery_saver"].toBool()));
            if (ds.contains("developer_options"))
                appendCommand(serial, AdbCommand::setDeveloperOptions(serial, ds["developer_options"].toBool()));
            if (ds.contains("adb_wifi"))
                appendCommand(serial, AdbCommand::setAdbWifi(serial, ds["adb_wifi"].toBool()));
            if (ds.contains("hidden_api_allow"))
                appendCommand(serial, AdbCommand::setHiddenApiAllow(serial, ds["hidden_api_allow"].toBool()));

            QJsonObject dp = config["display"].toObject();
            if (dp.contains("auto_rotate"))
                appendCommand(serial, AdbCommand::setAutoRotate(serial, dp["auto_rotate"].toBool()));
            if (dp.contains("show_touches"))
                appendCommand(serial, AdbCommand::setShowTouches(serial, dp["show_touches"].toBool()));
            if (dp.contains("pointer_location"))
                appendCommand(serial, AdbCommand::setPointerLocation(serial, dp["pointer_location"].toBool()));
            if (dp.contains("dark_mode"))
                appendCommand(serial, AdbCommand::setDarkMode(serial, dp["dark_mode"].toBool()));
            if (dp.contains("force_rtl"))
                appendCommand(serial, AdbCommand::setForceRtl(serial, dp["force_rtl"].toBool()));
            if (dp.contains("animations_enabled"))
                appendCommand(serial, AdbCommand::setAnimationsEnabled(serial, dp["animations_enabled"].toBool()));
        }

        if (commands.isEmpty()) {
            QMessageBox::information(uim->m_mainWindow, tr("Deploy Configuration"),
                                     tr("No configuration commands to run."));
            return;
        }

        uim->m_ui->devBtnDeployConfig->setEnabled(false);
        const QString adbPath = AdbManager::instance().getAdbPath();
        const QPointer<UiManager> owner(uim);
        (void)QtConcurrent::run([owner, adbPath, commands, deviceCount = serials.size()]() {
            QStringList failures;
            for (const auto &command : commands) {
                const AdbProcessResult result = AdbExecutor::run(adbPath, command.second, 10000);
                if (!result.succeeded()) {
                    failures << QStringLiteral("%1: %2")
                                    .arg(command.first, result.errorMessage());
                }
            }

            if (!owner)
                return;
            QMetaObject::invokeMethod(owner, [owner, failures, deviceCount]() {
                if (!owner)
                    return;
                UiManager *uim = owner.data();
                uim->m_ui->devBtnDeployConfig->setEnabled(true);
                if (failures.isEmpty()) {
                    QMessageBox::information(
                        uim->m_mainWindow, tr("Deploy Successful"),
                        tr("Configuration deployed to %1 device(s).").arg(deviceCount));
                    return;
                }

                QMessageBox::warning(
                    uim->m_mainWindow, tr("Deploy Completed With Errors"),
                    tr("Some commands failed:\n%1").arg(failures.join(QLatin1Char('\n'))));
            }, Qt::QueuedConnection);
        });
    });

    // ── Preset management ───────────────────────────────────────────────────

    // Save preset — shows input dialog to type a name
    connect(uim->m_ui->devBtnSavePreset, &QPushButton::clicked, uim,
            [uim, buildConfigJson]() {
        const QString name = PresetDialogs::askPresetName(
            uim->m_mainWindow,
            tr("Save Configuration Preset"),
            tr("Enter a name for this preset:"));
        if (name.isEmpty()) return;

        QJsonDocument doc(buildConfigJson());
        DevicesManager::instance().saveConfigPreset(name, doc.toJson(QJsonDocument::Compact));
        QMessageBox::information(uim->m_mainWindow, tr("Save Preset"),
                                 tr("Preset \"%1\" saved successfully.").arg(name));
    });

    // Load preset — shared dialog with delete support
    connect(uim->m_ui->devBtnLoadPreset, &QPushButton::clicked, uim,
            [uim, applyConfigJson]() {
        const QString name = PresetDialogs::pickPresetWithDelete(
            uim->m_mainWindow,
            tr("Load Configuration Preset"),
            tr("Select a preset to load:"),
            DevicesManager::instance().configPresetStore());
        if (name.isEmpty()) return;

        const QByteArray data = DevicesManager::instance().loadConfigPreset(name);
        if (data.isEmpty()) return;

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) return;

        applyConfigJson(doc.object());
        uim->m_ui->devJsonView->setPlainText(
            QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    });

}
