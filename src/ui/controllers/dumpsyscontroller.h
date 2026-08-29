#ifndef DUMPSYSCONTROLLER_H
#define DUMPSYSCONTROLLER_H

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

#include "textsearchmodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
class QStatusBar;
class QTimer;
class QTreeView;
QT_END_NAMESPACE

class HighlightDelegate;

// Owns the Dumpsys tab: service completer, search highlights and result list,
// raw command box. Decoupled from UiManager via a device-id provider callback.
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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

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

private:
    void buildToolbar();
    void buildPresetChips();
    /**
     * The result panel: a VS Code-style list, left of the output, of the lines
     * that contain the search term. Clicking a line jumps to it; the panel can
     * be hidden and remembers whether it was.
     */
    void buildResultsPanel();
    void setResultsPanelVisible(bool visible);
    void renderOutput();
    /**
     * Rebuild every ExtraSelection (diff lines + search hits) and the result
     * list in one document pass, and report the match count.
     *
     * Searching used to walk the whole document twice per keystroke — once to
     * count and once to build the selections — on output that is routinely
     * megabytes. One pass, debounced, and capped at kMaxSearchHighlights.
     */
    void refreshExtraSelections();
    /** Show fresh results, keeping the line the user picked selected. */
    void showResults(QVector<TextSearchModel::Result> results, int matchCount, bool truncated);
    /** Select @p row's first match in the output and bring it into view. */
    void jumpToResult(int row);
    /** Select the result on the output cursor's line, after find next / previous. */
    void syncResultsToCursor();
    void updateResultsSummary();
    /** Size the line-number column for the output's last line. */
    void updateResultsGutter();
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
    QTimer       *m_searchDebounce = nullptr;

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

    // Search result panel.
    QSplitter         *m_resultsSplitter    = nullptr;
    QWidget           *m_resultsPanel       = nullptr;
    QLabel            *m_resultsSummary     = nullptr;
    QTreeView         *m_resultsView        = nullptr;
    TextSearchModel   *m_resultsModel       = nullptr;
    HighlightDelegate *m_resultsHighlighter = nullptr;
    QPushButton       *m_btnResults         = nullptr;
    /** Line picked in the results; kept selected across re-renders. -1 = none. */
    int                m_selectedResultLine = -1;
    /** Set while the list is repopulated, so restoring a selection does not jump. */
    bool               m_restoringResults   = false;
    /** Set while renderOutput() replaces the output text. */
    bool               m_renderingOutput    = false;
};

#endif // DUMPSYSCONTROLLER_H
