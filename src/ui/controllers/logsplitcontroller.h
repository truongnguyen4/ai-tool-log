// LogSplitController: adds an optional second log pane (Pane B) with its own
// LogModel + table + MarkLogModel + mark table. A toolbar switch chooses which
// pane is the "active" target for filters, load, start/stop, clear and marks.
//
// While logcat capture is running, the switch is disabled (cannot flip).
//
// The controller only OWNS the widgets and data. UiManager is responsible for
// routing ingestion / filter / load operations to the active pane via the
// accessors below.
#pragma once

#include "logentry.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QVector>

class QSplitter;
class QTableView;
class QWidget;
class LogModel;
class MarkLogModel;

namespace Ui { class MainWindow; }

class LogSplitController : public QObject
{
    Q_OBJECT
public:
    // Per-pane mutable state, mirroring UiManager's existing per-pane fields.
    struct PaneState {
        QVector<LogEntry>   allLogs;
        QVector<LogEntry>   filteredLogs;
        QHash<quint64, int> allLogsIndex;
        QHash<quint64, int> filteredLogsIndex;
        QSet<int>           markedRows;
        quint64             nextLogId      = 0;
        LogModel           *model          = nullptr;
        MarkLogModel       *markModel      = nullptr;
        QTableView         *table          = nullptr;
        QTableView         *markTable      = nullptr;
    };

    explicit LogSplitController(Ui::MainWindow *ui, QObject *parent = nullptr);

    void setup();

    bool isSplit()      const { return m_splitActive; }
    bool activeIsB()    const { return m_splitActive && m_activeIsB; }

    PaneState *paneB()        { return m_splitActive ? &m_paneB : nullptr; }
    const PaneState *paneB() const { return m_splitActive ? &m_paneB : nullptr; }

public slots:
    void setSplitEnabled(bool enable);
    void setActivePaneB(bool b);
    void setSyncHighlight(bool on);

signals:
    void activePaneChanged(bool isB);
    void splitChanged(bool active);
    void paneBBuilt(QTableView *logTable, QTableView *markTable);

private:
    void buildPaneB();
    void teardownSplit();
    void installPaneFocusFilters();
    void applyActivePaneBorders();
    void wireColumnSync(QTableView *a, QTableView *b);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

    Ui::MainWindow              *m_ui                = nullptr;

    bool                         m_splitActive       = false;
    bool                         m_activeIsB         = false;
    bool                         m_syncHighlight     = false;

    QSplitter                   *m_logSplitterPanes   = nullptr;
    QSplitter                   *m_markSplitterPanes  = nullptr;
    QWidget                     *m_paneBLogContainer  = nullptr;
    QWidget                     *m_paneBMarkContainer = nullptr;

    PaneState                    m_paneB;

    QPointer<QSplitter>          m_originalLogParent;
    int                          m_logTableOriginalIndex  = 0;
    QPointer<QWidget>            m_originalMarkContainer;
    QPointer<QSplitter>          m_originalMarkParentSplitter;
    int                          m_markContainerOriginalIndex = 0;
};
