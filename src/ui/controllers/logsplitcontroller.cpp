#include "logsplitcontroller.h"
#include "ui_mainwindow.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "tablestyler.h"
#include "tableconfig.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr int kSplitterHandleWidth = 4;
constexpr int kMinPaneWidth        = 50;
constexpr QSize kClearButtonSize{22, 22};

/** Copy column widths and visibility from one table to another. */
void mirrorColumns(const QTableView *from, QTableView *to, int columnCount)
{
    for (int column = 0; column < columnCount; ++column) {
        to->setColumnHidden(column, from->isColumnHidden(column));
        to->setColumnWidth(column, from->columnWidth(column));
    }
}

/** Give a splitter an even two-way split, now and once layout has settled. */
void splitEvenly(QSplitter *splitter, QObject *context)
{
    const auto halve = [](QSplitter *target) {
        const int half = qMax(kMinPaneWidth, target->width() / 2);
        target->setSizes({half, half});
    };
    halve(splitter);
    QPointer<QSplitter> guarded(splitter);
    QTimer::singleShot(0, context, [guarded, halve]() {
        if (guarded)
            halve(guarded);
    });
}

/** Mark a table as the active or inactive pane; colours come from the theme. */
void setPaneState(QTableView *view, bool active)
{
    if (!view)
        return;
    view->setProperty("pane", active ? QStringLiteral("active")
                                     : QStringLiteral("inactive"));
    view->style()->unpolish(view);
    view->style()->polish(view);
}

} // namespace

LogSplitController::LogSplitController(Ui::MainWindow *ui, QObject *parent)
    : QObject(parent), m_ui(ui)
{
}

void LogSplitController::setup()
{
    if (!m_ui || !m_ui->btnSplitLog)
        return;
    connect(m_ui->btnSplitLog, &QPushButton::toggled,
            this, &LogSplitController::setSplitEnabled);

    // The active pane follows mouse focus on the tables.
    installPaneFocusFilters();
}

void LogSplitController::setSplitEnabled(bool enable)
{
    if (enable == m_splitActive)
        return;

    m_splitActive = enable;
    if (enable) {
        m_activeIsB = false;   // always start on pane A when (re-)enabling
        buildPaneB();
    } else {
        teardownSplit();
    }

    applyActivePaneBorders();
    emit splitChanged(enable);
    if (!enable)
        emit activePaneChanged(false);
}

void LogSplitController::setActivePaneB(bool b)
{
    if (!m_splitActive || b == m_activeIsB)
        return;
    m_activeIsB = b;
    applyActivePaneBorders();
    emit activePaneChanged(b);
}

void LogSplitController::setSyncHighlight(bool on)
{
    if (m_syncHighlight == on)
        return;
    m_syncHighlight = on;
    applyActivePaneBorders();
}

void LogSplitController::installPaneFocusFilters()
{
    if (m_ui->tableLog)     m_ui->tableLog->installEventFilter(this);
    if (m_ui->tableMarkLog) m_ui->tableMarkLog->installEventFilter(this);
}

void LogSplitController::applyActivePaneBorders()
{
    // Sync mode accents both panes, to show they operate in lockstep.
    const bool paneAActive = !m_splitActive ? false
                             : (m_syncHighlight || !m_activeIsB);
    const bool paneBActive = m_splitActive && (m_syncHighlight || m_activeIsB);

    setPaneState(m_ui->tableLog,     paneAActive);
    setPaneState(m_ui->tableMarkLog, paneAActive);
    setPaneState(m_paneB.table,      paneBActive);
    setPaneState(m_paneB.markTable,  paneBActive);
}

bool LogSplitController::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_splitActive
        || (event->type() != QEvent::FocusIn && event->type() != QEvent::MouseButtonPress))
        return QObject::eventFilter(watched, event);

    if (watched == m_ui->tableLog || watched == m_ui->tableMarkLog)
        setActivePaneB(false);
    else if (watched == m_paneB.table || watched == m_paneB.markTable)
        setActivePaneB(true);

    return QObject::eventFilter(watched, event);
}

// ─────────────────────────────────────────────────────────────────────────────
// Pane B construction
// ─────────────────────────────────────────────────────────────────────────────

void LogSplitController::buildPaneB()
{
    if (!m_ui || !m_ui->tableLog)
        return;

    auto *parentSplitter = qobject_cast<QSplitter *>(m_ui->tableLog->parentWidget());
    if (!parentSplitter) {
        m_splitActive = false;
        return;
    }

    buildPaneBLogTable(parentSplitter);
    buildPaneBMarkTable();

    emit paneBBuilt(m_paneB.table, m_paneB.markTable);

    if (m_paneB.table)     m_paneB.table->installEventFilter(this);
    if (m_paneB.markTable) m_paneB.markTable->installEventFilter(this);
    wireColumnSync(m_ui->tableLog,     m_paneB.table);
    wireColumnSync(m_ui->tableMarkLog, m_paneB.markTable);

    m_activeIsB = false;
}

void LogSplitController::buildPaneBLogTable(QSplitter *parentSplitter)
{
    using namespace TableConfig::LogColumns;

    QTableView *paneATable = m_ui->tableLog;
    m_originalLogParent     = parentSplitter;
    m_logTableOriginalIndex = parentSplitter->indexOf(paneATable);

    m_logSplitterPanes = new QSplitter(Qt::Horizontal, parentSplitter);
    m_logSplitterPanes->setChildrenCollapsible(false);
    m_logSplitterPanes->setHandleWidth(kSplitterHandleWidth);

    m_paneBLogContainer = new QWidget(m_logSplitterPanes);
    auto *layout = new QVBoxLayout(m_paneBLogContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_paneB.table = new QTableView(m_paneBLogContainer);
    m_paneB.table->setObjectName(QStringLiteral("tableLogB"));
    TableStyler::applyLogTableStyle({m_paneB.table});
    layout->addWidget(m_paneB.table, /*stretch=*/1);

    m_paneB.model = new LogModel(this);
    m_paneB.model->setMarkedRows(&m_paneB.markedRows);
    m_paneB.table->setModel(m_paneB.model);

    mirrorColumns(paneATable, m_paneB.table, TOTAL_COLUMNS);
    m_paneB.table->horizontalHeader()->setSectionResizeMode(MESSAGE, QHeaderView::Stretch);

    // Re-parent pane A next to pane B inside the new horizontal splitter.
    paneATable->setParent(nullptr);
    m_logSplitterPanes->addWidget(paneATable);
    m_logSplitterPanes->addWidget(m_paneBLogContainer);
    parentSplitter->insertWidget(m_logTableOriginalIndex, m_logSplitterPanes);
    splitEvenly(m_logSplitterPanes, this);
}

void LogSplitController::buildPaneBMarkTable()
{
    using namespace TableConfig::LogColumns;

    QWidget *markContainer = m_ui->markLogContainer;
    if (!markContainer)
        return;
    auto *markParentSplitter = qobject_cast<QSplitter *>(markContainer->parentWidget());
    if (!markParentSplitter)
        return;

    m_originalMarkContainer      = markContainer;
    m_originalMarkParentSplitter = markParentSplitter;
    m_markContainerOriginalIndex = markParentSplitter->indexOf(markContainer);

    m_markSplitterPanes = new QSplitter(Qt::Horizontal, markParentSplitter);
    m_markSplitterPanes->setChildrenCollapsible(false);
    m_markSplitterPanes->setHandleWidth(kSplitterHandleWidth);

    m_paneBMarkContainer = new QWidget(m_markSplitterPanes);
    auto *layout = new QVBoxLayout(m_paneBMarkContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header mirrors pane A's. Re-using the same object names lets the theme
    // sheet style pane B identically with no duplicated rules.
    auto *header = new QWidget(m_paneBMarkContainer);
    header->setObjectName(QStringLiteral("markLogHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 4, 0);
    headerLayout->setSpacing(2);

    auto *label = new QLabel(tr("Marked"), header);
    label->setObjectName(QStringLiteral("lblMarkLog"));
    headerLayout->addWidget(label);
    headerLayout->addStretch(1);

    auto *clearButton = new QPushButton(header);
    clearButton->setObjectName(QStringLiteral("btnClearAllMarked"));
    clearButton->setIcon(QIcon(QStringLiteral(":/icons/edit-delete.svg")));
    clearButton->setFixedSize(kClearButtonSize);
    clearButton->setToolTip(tr("Clear all marked logs in this pane"));
    headerLayout->addWidget(clearButton);
    layout->addWidget(header);

    m_paneBConnections << connect(clearButton, &QPushButton::clicked, this, [this]() {
        if (!m_paneB.markModel || !m_paneB.model)
            return;
        m_paneB.markedRows.clear();
        m_paneB.markModel->clear();
        m_paneB.model->setMarkedRows(&m_paneB.markedRows);
    });

    m_paneB.markTable = new QTableView(m_paneBMarkContainer);
    m_paneB.markTable->setObjectName(QStringLiteral("tableMarkLogB"));
    TableStyler::applyLogTableStyle({m_paneB.markTable});
    layout->addWidget(m_paneB.markTable, /*stretch=*/1);

    m_paneB.markModel = new MarkLogModel(this);
    m_paneB.markTable->setModel(m_paneB.markModel);
    mirrorColumns(m_ui->tableMarkLog, m_paneB.markTable, TOTAL_COLUMNS);
    m_paneB.markTable->horizontalHeader()->setSectionResizeMode(MESSAGE, QHeaderView::Stretch);

    markContainer->setParent(nullptr);
    m_markSplitterPanes->addWidget(markContainer);
    m_markSplitterPanes->addWidget(m_paneBMarkContainer);
    markParentSplitter->insertWidget(m_markContainerOriginalIndex, m_markSplitterPanes);
    splitEvenly(m_markSplitterPanes, this);
}

void LogSplitController::wireColumnSync(QTableView *a, QTableView *b)
{
    if (!a || !b)
        return;
    QHeaderView *headerA = a->horizontalHeader();
    QHeaderView *headerB = b->horizontalHeader();
    if (!headerA || !headerB)
        return;

    const auto mirror = [this](QPointer<QTableView> destination) {
        return [this, destination](int column, int /*oldSize*/, int newSize) {
            // Setting the width on the far table emits sectionResized again;
            // the flag stops the two headers from bouncing off each other.
            if (m_syncingColumnWidths || !destination)
                return;
            m_syncingColumnWidths = true;
            if (destination->columnWidth(column) != newSize)
                destination->setColumnWidth(column, newSize);
            m_syncingColumnWidths = false;
        };
    };

    m_paneBConnections
        << connect(headerA, &QHeaderView::sectionResized, this, mirror(QPointer<QTableView>(b)))
        << connect(headerB, &QHeaderView::sectionResized, this, mirror(QPointer<QTableView>(a)));
}

void LogSplitController::teardownSplit()
{
    // Drop pane-B-scoped connections first: pane A's header outlives the
    // split, so leaving them attached would stack a new pair on every cycle.
    for (const QMetaObject::Connection &connection : std::as_const(m_paneBConnections))
        disconnect(connection);
    m_paneBConnections.clear();

    // Restore the log table to its original parent.
    if (m_logSplitterPanes && m_originalLogParent && m_ui->tableLog) {
        QTableView *table = m_ui->tableLog;
        table->setParent(nullptr);
        m_originalLogParent->insertWidget(m_logTableOriginalIndex, table);
        m_logSplitterPanes->deleteLater();
    }
    m_logSplitterPanes  = nullptr;
    m_paneBLogContainer = nullptr;

    // Restore the mark-log container.
    if (m_markSplitterPanes && m_originalMarkContainer && m_originalMarkParentSplitter) {
        QWidget *container = m_originalMarkContainer.data();
        container->setParent(nullptr);
        m_originalMarkParentSplitter->insertWidget(m_markContainerOriginalIndex, container);
        m_markSplitterPanes->deleteLater();
    }
    m_markSplitterPanes  = nullptr;
    m_paneBMarkContainer = nullptr;

    // The pane B widgets die with their containers, but the models are
    // parented to this controller and would otherwise survive — holding on to
    // a full copy of the log buffer — for every split/unsplit cycle.
    delete m_paneB.model;
    delete m_paneB.markModel;
    m_paneB = PaneState();
    m_activeIsB = false;
}
