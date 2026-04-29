#ifndef CRADLECONTROLLER_H
#define CRADLECONTROLLER_H

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QStatusBar;
QT_END_NAMESPACE

// Owns all Cradle Manager tab logic. Reads the current device ID lazily via
// the supplied callback so it stays decoupled from UiManager state.
class CradleController : public QObject
{
    Q_OBJECT
public:
    using DeviceIdProvider = std::function<QString()>;

    CradleController(Ui::MainWindow *ui,
                     QStatusBar *statusBar,
                     DeviceIdProvider deviceIdProvider,
                     QObject *parent = nullptr);

    // Wires every Cradle tab signal/slot. Call once after UI setup.
    void setup();

private slots:
    void onCradleGetInfo();
    void onCradleQueryFirmware();
    void onCradleUpdateFirmware();
    void onCradleQuerySchedule();
    void onCradleCommandFinished(const QString &output, const QString &error);

private:
    void runCradle(const QStringList &args, QWidget *pendingWidget);

    Ui::MainWindow   *m_ui;
    QStatusBar       *m_statusBar;
    DeviceIdProvider  m_deviceIdProvider;
};

#endif // CRADLECONTROLLER_H
