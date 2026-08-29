#ifndef DUMPSYSCONTROLLER_H
#define DUMPSYSCONTROLLER_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStatusBar;
class QTimer;
QT_END_NAMESPACE

// Owns the Dumpsys tab: service completer, search highlights, raw command box.
// Decoupled from UiManager via a device-id provider callback.
class DumpsysController : public QObject
{
    Q_OBJECT
public:
    using DeviceIdProvider = std::function<QString()>;

    DumpsysController(Ui::MainWindow *ui,
                      QStatusBar *statusBar,
                      DeviceIdProvider deviceIdProvider,
                      QObject *parent = nullptr);

    void setup();

    // Called from device-change paths in UiManager.
    void clearServices() { m_dumpsysServices.clear(); }
    void refreshCommandText() { updateDumpsysCommandText(); }
    void restoreLastService(const QString &deviceId);
    void stopMonitor(); // turns off the Monitor button if active

private slots:
    void updateDumpsysCommandText();
    void onRunDumpsysClicked();
    void onDumpsysFetched(const QString &output);
    void onDumpsysSearchChanged();
    void onDumpsysSearchNext();
    void onDumpsysSearchPrev();
    void onRunDumpsysCmdClicked();
    void onRawAdbCommandFinished(const QString &output);
    void onDumpsysListFetched(const QStringList &services);

    void onSaveOutputClicked();
    void onSnapshotClicked();
    void onDiffToggled(bool on);
    void onPresetClicked(const QString &service);
    void onMonitorToggled(bool on);

private:
    void buildToolbar();
    void buildPresetChips();
    void renderOutput();
    /**
     * Rebuild every ExtraSelection (diff lines + search hits) in one document
     * pass and report the match count.
     *
     * Searching used to walk the whole document twice per keystroke — once to
     * count and once to build the selections — on output that is routinely
     * megabytes. One pass, debounced, and capped at kMaxSearchHighlights.
     */
    void refreshExtraSelections();
    /** Move the cursor to the first / next / previous match. */
    void findInOutput(bool backwards, bool fromStart = false);
    QString currentDumpsysArgs() const;
    QString computeDiff(const QString &a, const QString &b);

    Ui::MainWindow   *m_ui;
    QStatusBar       *m_statusBar;
    DeviceIdProvider  m_deviceIdProvider;
    QStringList       m_dumpsysServices;

    // Toolbar widgets injected at runtime.
    QLabel       *m_matchLabel  = nullptr;
    QLineEdit    *m_packageInput = nullptr;   ///< optional dumpsys package argument
    QPushButton  *m_btnSave     = nullptr;
    QPushButton  *m_btnSnapshot = nullptr;
    QPushButton  *m_btnDiff     = nullptr;
    QPushButton  *m_btnMonitor  = nullptr;
    QComboBox    *m_monitorIntervalCombo = nullptr;
    QTimer       *m_searchDebounce = nullptr;

    // Monitor mode: re-fetch the current dumpsys service every N ms.
    QTimer       *m_monitorTimer = nullptr;
    bool          m_monitoring   = false;

    // Diff state.
    QString               m_currentOutput;        // last raw output (for diff toggle)
    QString               m_currentService;       // last fetched service
    QHash<QString, QString> m_snapshots;          // per-service snapshot text
    bool                  m_diffMode = false;

    // Diff highlight state (line indices in rendered text).
    QList<int>            m_diffAddedLines;
    QList<int>            m_diffRemovedLines;
    // Search-extra-selections cache so diff highlights survive search updates.
    QString               m_lastSearchNeedle;
};

#endif // DUMPSYSCONTROLLER_H
