#include "cradlecontroller.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"

#include <QStatusBar>
#include <QPushButton>
#include <QLineEdit>
#include <QRadioButton>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QLabel>

CradleController::CradleController(Ui::MainWindow *ui,
                                   QStatusBar *statusBar,
                                   DeviceIdProvider deviceIdProvider,
                                   QObject *parent)
    : QObject(parent)
    , m_ui(ui)
    , m_statusBar(statusBar)
    , m_deviceIdProvider(std::move(deviceIdProvider))
{}

void CradleController::setup()
{
    connect(m_ui->radioCustomFirmware, &QRadioButton::toggled, this, [this](bool checked) {
        m_ui->txtCradleFwPath->setEnabled(checked);
    });

    connect(m_ui->btnCradleGet,            &QPushButton::clicked,    this, &CradleController::onCradleGetInfo);
    connect(m_ui->txtCradleKey,            &QLineEdit::returnPressed, this, &CradleController::onCradleGetInfo);
    connect(m_ui->btnCradleQueryFirmware,  &QPushButton::clicked,    this, &CradleController::onCradleQueryFirmware);
    connect(m_ui->btnCradleUpdateFirmware, &QPushButton::clicked,    this, &CradleController::onCradleUpdateFirmware);
    connect(m_ui->btnCradleQuerySchedule,  &QPushButton::clicked,    this, &CradleController::onCradleQuerySchedule);
    connect(m_ui->btnCradleClearOutput,    &QPushButton::clicked,
            this, [this]() { m_ui->txtCradleOutput->clear(); m_ui->lblCradleLastCmd->setText("—"); });

    connect(&AdbManager::instance(), &AdbManager::cradleCommandFinished,
            this, &CradleController::onCradleCommandFinished);
}

void CradleController::runCradle(const QStringList &args, QWidget *pendingWidget)
{
    m_ui->lblCradleLastCmd->setText("adb shell cmd cradle_manager " + args.join(' '));
    if (pendingWidget) pendingWidget->setEnabled(false);
    AdbManager::instance().runCradleCommand(m_deviceIdProvider(), args);
}

void CradleController::onCradleGetInfo()
{
    if (m_deviceIdProvider().isEmpty()) { m_statusBar->showMessage("No device selected", 3000); return; }
    QStringList args = { "get" };
    const QString key = m_ui->txtCradleKey->text().trimmed();
    if (!key.isEmpty()) args << key;
    runCradle(args, m_ui->btnCradleGet);
}

void CradleController::onCradleQueryFirmware()
{
    if (m_deviceIdProvider().isEmpty()) { m_statusBar->showMessage("No device selected", 3000); return; }
    QStringList args = { "query-firmware" };
    if (m_ui->radioDefaultFirmware->isChecked()) {
        args << "--default-firmware";
    } else {
        const QString path = m_ui->txtCradleFwPath->text().trimmed();
        if (path.isEmpty()) { m_statusBar->showMessage("Please enter a firmware file path", 3000); return; }
        args << "--path" << path;
    }
    runCradle(args, m_ui->btnCradleQueryFirmware);
}

void CradleController::onCradleUpdateFirmware()
{
    if (m_deviceIdProvider().isEmpty()) { m_statusBar->showMessage("No device selected", 3000); return; }
    QStringList args = { "update-firmware" };
    if (m_ui->radioDefaultFirmware->isChecked()) {
        args << "--default-firmware";
    } else {
        const QString path = m_ui->txtCradleFwPath->text().trimmed();
        if (path.isEmpty()) { m_statusBar->showMessage("Please enter a firmware file path", 3000); return; }
        args << "--path" << path;
    }
    QStringList types;
    if (m_ui->chkFwTypeApplication->isChecked()) types << "Application";
    if (m_ui->chkFwTypeBootloader->isChecked())  types << "Bootloader";
    if (m_ui->chkFwTypePreloader->isChecked())   types << "Preloader";
    if (m_ui->chkFwTypeWlc->isChecked())         types << "WLC";
    if (!types.isEmpty()) args << "--type" << types;
    runCradle(args, m_ui->btnCradleUpdateFirmware);
}

void CradleController::onCradleQuerySchedule()
{
    if (m_deviceIdProvider().isEmpty()) { m_statusBar->showMessage("No device selected", 3000); return; }
    QStringList days;
    if (m_ui->chkCradleMon->isChecked()) days << "1";
    if (m_ui->chkCradleTue->isChecked()) days << "2";
    if (m_ui->chkCradleWed->isChecked()) days << "3";
    if (m_ui->chkCradleThu->isChecked()) days << "4";
    if (m_ui->chkCradleFri->isChecked()) days << "5";
    if (m_ui->chkCradleSat->isChecked()) days << "6";
    if (m_ui->chkCradleSun->isChecked()) days << "7";
    if (days.isEmpty()) { m_statusBar->showMessage("Please select at least one day", 3000); return; }
    QStringList args = { "query-schedule", "-d" };
    args << days;
    runCradle(args, m_ui->btnCradleQuerySchedule);
}

void CradleController::onCradleCommandFinished(const QString &output, const QString &error)
{
    for (QPushButton *btn : {m_ui->btnCradleGet, m_ui->btnCradleQueryFirmware,
                             m_ui->btnCradleUpdateFirmware, m_ui->btnCradleQuerySchedule})
        btn->setEnabled(true);

    if (!error.isEmpty()) {
        m_ui->txtCradleOutput->appendPlainText("[ERROR]\n" + error);
        if (!output.isEmpty())
            m_ui->txtCradleOutput->appendPlainText("[OUTPUT]\n" + output);
    } else {
        m_ui->txtCradleOutput->appendPlainText(output.isEmpty() ? "(no output)" : output);
    }

    QTextCursor c = m_ui->txtCradleOutput->textCursor();
    c.movePosition(QTextCursor::End);
    m_ui->txtCradleOutput->setTextCursor(c);
    m_statusBar->showMessage("Cradle command completed", 2000);
}
