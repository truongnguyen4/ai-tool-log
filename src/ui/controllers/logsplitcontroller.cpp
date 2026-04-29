#include "logsplitcontroller.h"
#include "ui_mainwindow.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "tableconfig.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QEvent>

namespace {

void styleLogTable(QTableView *tv)
{
    tv->setAlternatingRowColors(true);
    tv->setSelectionBehavior(QAbstractItemView::SelectRows);
    tv->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tv->setSortingEnabled(false);
    tv->setShowGrid(false);
    tv->setFrameShape(QFrame::NoFrame);
    tv->setWordWrap(true);
    tv->verticalHeader()->setVisible(false);
    tv->verticalHeader()->setDefaultSectionSize(26);
    tv->verticalHeader()->setMinimumSectionSize(22);
    tv->horizontalHeader()->setHighlightSections(false);
    tv->horizontalHeader()->setMinimumSectionSize(48);
    tv->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    tv->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

void mirrorColumnWidths(QTableView *src, QTableView *dst, int totalCols)
{
    for (int col = 0; col < totalCols; ++col) {
        dst->setColumnHidden(col, src->isColumnHidden(col));
        dst->setColumnWidth(col, src->columnWidth(col));
    }
}

} // namespace

LogSplitController::LogSplitController(Ui::MainWindow *ui, QObject *parent)
    : QObject(parent), m_ui(ui)
{
}

void LogSplitController::setup()
{
    if (!m_ui || !m_ui->btnSplitLog) return;
    connect(m_ui->btnSplitLog, &QPushButton::toggled,
            this, &LogSplitController::setSplitEnabled);

    // Active pane is now driven entirely by mouse focus on the table.
    installPaneFocusFilters();
}

void LogSplitController::setSplitEnabled(bool enable)
{
    if (enable == m_splitActive) return;
    if (enable) {
        m_activeIsB = false;     // always reset to A when re-enabling
        buildPaneB();
    } else {
        teardownSplit();
    }
    m_splitActive = enable;
    emit splitChanged(enable);
    if (!enable) emit activePaneChanged(false);
    applyActivePaneBorders();
}

void LogSplitController::setActivePaneB(bool b)
{
    if (!m_splitActive || b == m_activeIsB) return;
    m_activeIsB = b;
    applyActivePaneBorders();
    emit activePaneChanged(b);
}

void LogSplitController::installPaneFocusFilters()
{
    // Focus / mouse-press on either pane's tables makes that pane active.
    if (m_ui->tableLog)     m_ui->tableLog->installEventFilter(this);
    if (m_ui->tableMarkLog) m_ui->tableMarkLog->installEventFilter(this);
}

void LogSplitController::applyActivePaneBorders()
{
    // Indigo accent for the active pane border. Inactive = transparent border
    // of the same width so geometry doesn't shift when switching.
    static const char *kActive   =
        "QTableView { border: 2px solid #818cf8; border-radius: 4px; }";
    static const char *kInactive =
        "QTableView { border: 2px solid transparent; border-radius: 4px; }";
    // Sync highlight: keep the same indigo accent as the active pane, but
    // apply it to BOTH panes so the user sees they operate in lockstep.
    static const char *kSync = kActive;

    auto styleOne = [](QTableView *tv, const char *qss) {
        if (!tv) return;
        tv->setStyleSheet(QString::fromLatin1(qss));
    };

    if (!m_splitActive) {
        styleOne(m_ui->tableLog,     kInactive);
        styleOne(m_ui->tableMarkLog, kInactive);
        styleOne(m_paneB.table,      kInactive);
        styleOne(m_paneB.markTable,  kInactive);
        return;
    }
    if (m_syncHighlight) {
        styleOne(m_ui->tableLog,     kSync);
        styleOne(m_ui->tableMarkLog, kSync);
        styleOne(m_paneB.table,      kSync);
        styleOne(m_paneB.markTable,  kSync);
        return;
    }
    const bool aActive = !m_activeIsB;
    styleOne(m_ui->tableLog,     aActive ? kActive : kInactive);
    styleOne(m_ui->tableMarkLog, aActive ? kActive : kInactive);
    styleOne(m_paneB.table,      aActive ? kInactive : kActive);
    styleOne(m_paneB.markTable,  aActive ? kInactive : kActive);
}

void LogSplitController::setSyncHighlight(bool on)
{
    if (m_syncHighlight == on) return;
    m_syncHighlight = on;
    applyActivePaneBorders();
}

bool LogSplitController::eventFilter(QObject *watched, QEvent *event)
{
    if (!m_splitActive) return QObject::eventFilter(watched, event);
    if (event->type() != QEvent::FocusIn && event->type() != QEvent::MouseButtonPress)
        return QObject::eventFilter(watched, event);

    if (watched == m_ui->tableLog || watched == m_ui->tableMarkLog) {
        if (m_activeIsB) setActivePaneB(false);
    } else if (watched == m_paneB.table || watched == m_paneB.markTable) {
        if (!m_activeIsB) setActivePaneB(true);
    }
    return QObject::eventFilter(watched, event);
}

void LogSplitController::buildPaneB()
{
    if (!m_ui || !m_ui->tableLog) return;

    using namespace TableConfig::LogColumns;

    // ── Wrap tableLog with a horizontal splitter ────────────────────────────
    auto *table = m_ui->tableLog;
    auto *parentSplitter = qobject_cast<QSplitter *>(table->parentWidget());
    if (!parentSplitter) return;

    m_originalLogParent      = parentSplitter;
    m_logTableOriginalIndex  = parentSplitter->indexOf(table);

    m_logSplitterPanes = new QSplitter(Qt::Horizontal, parentSplitter);
    m_logSplitterPanes->setChildrenCollapsible(false);
    m_logSplitterPanes->setHandleWidth(4);

    // Pane B log container: just the table (switch lives in toolbar now).
    m_paneBLogContainer = new QWidget(m_logSplitterPanes);
    auto *vb = new QVBoxLayout(m_paneBLogContainer);
    vb->setContentsMargins(0, 0, 0, 0);
    vb->setSpacing(0);

    m_paneB.table = new QTableView(m_paneBLogContainer);
    m_paneB.table->setObjectName(QStringLiteral("tableLogB"));
    styleLogTable(m_paneB.table);
    vb->addWidget(m_paneB.table, /*stretch=*/1);

    // Pane B's own LogModel
    m_paneB.model = new LogModel(this);
    m_paneB.model->setMarkedRows(&m_paneB.markedRows);
    m_paneB.table->setModel(m_paneB.model);

    // Mirror column setup from pane A
    mirrorColumnWidths(m_ui->tableLog, m_paneB.table, TOTAL_COLUMNS);
    m_paneB.table->horizontalHeader()->setSectionResizeMode(MESSAGE, QHeaderView::Stretch);

    // Reattach pane A and add pane B
    table->setParent(nullptr);
    m_logSplitterPanes->addWidget(table);
    m_logSplitterPanes->addWidget(m_paneBLogContainer);
    parentSplitter->insertWidget(m_logTableOriginalIndex, m_logSplitterPanes);
    // Equal 50/50 split based on current width
    const int half = qMax(50, m_logSplitterPanes->width() / 2);
    m_logSplitterPanes->setSizes({half, half});
    QPointer<QSplitter> sp(m_logSplitterPanes);
    QTimer::singleShot(0, this, [sp]() {
        if (sp) {
            const int h = qMax(50, sp->width() / 2);
            sp->setSizes({h, h});
        }
    });

    // ── Mark log: split too ─────────────────────────────────────────────────
    if (m_ui->markLogContainer) {
        auto *markContainer  = m_ui->markLogContainer;
        auto *markParentSpl  = qobject_cast<QSplitter *>(markContainer->parentWidget());
        if (markParentSpl) {
            m_originalMarkContainer       = markContainer;
            m_originalMarkParentSplitter  = markParentSpl;
            m_markContainerOriginalIndex  = markParentSpl->indexOf(markContainer);

            m_markSplitterPanes = new QSplitter(Qt::Horizontal, markParentSpl);
            m_markSplitterPanes->setChildrenCollapsible(false);
            m_markSplitterPanes->setHandleWidth(4);

            m_paneBMarkContainer = new QWidget(m_markSplitterPanes);
            auto *mvb = new QVBoxLayout(m_paneBMarkContainer);
            mvb->setContentsMargins(0, 0, 0, 0);
            mvb->setSpacing(0);

            // Header: "Marked" label + clear button (mirrors pane A header).
            // Re-using the exact object names so the existing theme stylesheet
            // (#markLogHeader / #lblMarkLog / #btnClearAllMarked rules) styles
            // pane B identically without any duplication.
            auto *mHeader = new QWidget(m_paneBMarkContainer);
            mHeader->setObjectName(QStringLiteral("markLogHeader"));
            auto *mhb = new QHBoxLayout(mHeader);
            mhb->setContentsMargins(0, 0, 4, 0);
            mhb->setSpacing(2);
            auto *mLabel = new QLabel(tr("Marked"), mHeader);
            mLabel->setObjectName(QStringLiteral("lblMarkLog"));
            mhb->addWidget(mLabel);
            mhb->addStretch(1);
            auto *btnClear = new QPushButton(mHeader);
            btnClear->setObjectName(QStringLiteral("btnClearAllMarked"));
            btnClear->setIcon(QIcon(QStringLiteral(":/icons/edit-delete.svg")));
            btnClear->setFixedSize(22, 22);
            btnClear->setToolTip(tr("Clear all marked logs in Pane B"));
            mhb->addWidget(btnClear);
            mvb->addWidget(mHeader);

            connect(btnClear, &QPushButton::clicked, this, [this]() {
                if (!m_paneB.markModel || !m_paneB.model) return;
                m_paneB.markedRows.clear();
                m_paneB.markModel->clear();
                m_paneB.model->setMarkedRows(&m_paneB.markedRows);
            });

            m_paneB.markTable = new QTableView(m_paneBMarkContainer);
            m_paneB.markTable->setObjectName(QStringLiteral("tableMarkLogB"));
            styleLogTable(m_paneB.markTable);
            mvb->addWidget(m_paneB.markTable, /*stretch=*/1);

            m_paneB.markModel = new MarkLogModel(this);
            m_paneB.markTable->setModel(m_paneB.markModel);
            mirrorColumnWidths(m_ui->tableMarkLog, m_paneB.markTable, TOTAL_COLUMNS);
            m_paneB.markTable->horizontalHeader()->setSectionResizeMode(MESSAGE,
                                                                        QHeaderView::Stretch);

            markContainer->setParent(nullptr);
            m_markSplitterPanes->addWidget(markContainer);
            m_markSplitterPanes->addWidget(m_paneBMarkContainer);
            markParentSpl->insertWidget(m_markContainerOriginalIndex, m_markSplitterPanes);
            const int mh = qMax(50, m_markSplitterPanes->width() / 2);
            m_markSplitterPanes->setSizes({mh, mh});
            QPointer<QSplitter> msp(m_markSplitterPanes);
            QTimer::singleShot(0, this, [msp]() {
                if (msp) {
                    const int h = qMax(50, msp->width() / 2);
                    msp->setSizes({h, h});
                }
            });
        }
    }

    emit paneBBuilt(m_paneB.table, m_paneB.markTable);
    if (m_paneB.table)     m_paneB.table->installEventFilter(this);
    if (m_paneB.markTable) m_paneB.markTable->installEventFilter(this);
    wireColumnSync(m_ui->tableLog,     m_paneB.table);
    wireColumnSync(m_ui->tableMarkLog, m_paneB.markTable);
    setActivePaneB(false);
    applyActivePaneBorders();
    emit splitChanged(true);
}

void LogSplitController::wireColumnSync(QTableView *a, QTableView *b)
{
    if (!a || !b) return;
    auto *ha = a->horizontalHeader();
    auto *hb = b->horizontalHeader();
    if (!ha || !hb) return;

    // Use guarded pointers so resizes from pane A after pane B is destroyed
    // (unsplit) don't deref a dangling table.
    auto *guard = new bool(false);
    QPointer<QTableView> pa = a;
    QPointer<QTableView> pb = b;
    connect(this, &QObject::destroyed, [guard]() { delete guard; });

    auto syncResize = [guard](QPointer<QTableView> src, QPointer<QTableView> dst) {
        return [guard, src, dst](int col, int /*oldSize*/, int newSize) {
            if (*guard) return;
            if (!src || !dst) return;
            *guard = true;
            if (dst->columnWidth(col) != newSize) dst->setColumnWidth(col, newSize);
            *guard = false;
        };
    };

    connect(ha, &QHeaderView::sectionResized, this, syncResize(pa, pb));
    connect(hb, &QHeaderView::sectionResized, this, syncResize(pb, pa));
}

void LogSplitController::teardownSplit()
{
    // ── Restore log table ───────────────────────────────────────────────────
    if (m_logSplitterPanes && m_originalLogParent && m_ui->tableLog) {
        auto *table = m_ui->tableLog;
        table->setParent(nullptr);
        m_originalLogParent->insertWidget(m_logTableOriginalIndex, table);
        m_logSplitterPanes->deleteLater();
    }
    m_logSplitterPanes  = nullptr;
    m_paneBLogContainer = nullptr;

    // ── Restore mark container ──────────────────────────────────────────────
    if (m_markSplitterPanes && m_originalMarkContainer && m_originalMarkParentSplitter) {
        auto *mc = m_originalMarkContainer.data();
        mc->setParent(nullptr);
        m_originalMarkParentSplitter->insertWidget(m_markContainerOriginalIndex, mc);
        m_markSplitterPanes->deleteLater();
    }
    m_markSplitterPanes  = nullptr;
    m_paneBMarkContainer = nullptr;

    // Models / table pointers belong to deleted widgets; null out.
    m_paneB = PaneState();
    m_activeIsB = false;
}
