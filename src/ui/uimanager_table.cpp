// UiManager: log/markLog table context menu + click handlers.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "filterengine.h"
#include "logmodel.h"
#include "logsplitcontroller.h"
#include "marklogmodel.h"
#include "tableconfig.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>

namespace {
class CloseOnDeactivateFilter : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override {
        if (ev->type() == QEvent::WindowDeactivate) {
            if (auto *w = qobject_cast<QWidget *>(obj))
                w->close();
            return true;
        }
        return QObject::eventFilter(obj, ev);
    }
};
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Table Interaction
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onTableContextMenu(const QPoint &pos)
{
    // Resolve which table/model fired the menu (Pane A vs Pane B).
    auto *senderTable = qobject_cast<QTableView *>(sender());
    if (!senderTable) senderTable = m_ui->tableLog;

    auto *pb = m_logSplitController ? m_logSplitController->paneB() : nullptr;
    const bool isPaneB = pb && pb->table && senderTable == pb->table;
    LogModel *model = isPaneB ? pb->model : m_logModel;

    const QModelIndex index = senderTable->indexAt(pos);
    if (!index.isValid()) return;

    const int column = index.column();
    const QString value = model->data(index, Qt::DisplayRole).toString();

    QMenu menu(m_mainWindow);

    // ── "View Cell Content" — available for every column ────────────────────
    QAction *viewCellAction = menu.addAction(tr("View Cell Content"));

    // ── Filter shortcuts — only for filterable columns ──────────────────────
    QAction *addOrAction  = nullptr;
    QAction *addAndAction = nullptr;
    QString filterType;

    using namespace TableConfig::LogColumns;
    switch (column) {
    case PID:     filterType = "pid";     break;
    case TID:     filterType = "tid";     break;
    case PACKAGE: filterType = "package"; break;
    case TAG:     filterType = "tag";     break;
    default:      break;
    }

    if (!filterType.isEmpty()) {
        const QString displayName = filterType.toUpper().at(0) + filterType.mid(1);
        menu.addSeparator();
        addOrAction  = menu.addAction(
            QString("Add '%1' to %2 filter (OR)").arg(value, displayName));
        addAndAction = menu.addAction(
            QString("Add '%1' to %2 filter (AND)").arg(value, displayName));
    }

    const QAction *chosen = menu.exec(senderTable->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == viewCellAction) {
        showCellContentDialog(value, senderTable);
    } else if (chosen == addOrAction) {
        addToFilter(filterType, value, FilterOperator::OR);
    } else if (chosen == addAndAction) {
        addToFilter(filterType, value, FilterOperator::AND);
    }
}

void UiManager::addToFilter(const QString &filterType, const QString &value, FilterOperator op)
{
    QLineEdit *filterField = nullptr;
    if      (filterType == "tag")     filterField = m_ui->txtTagFilter;
    else if (filterType == "package") filterField = m_ui->txtPackageFilter;
    else if (filterType == "pid")     filterField = m_ui->txtPidFilter;
    else if (filterType == "tid")     filterField = m_ui->txtPidFilter; // no separate TID field
    if (!filterField) return;

    const QString current   = filterField->text().trimmed();
    const QString separator = (op == FilterOperator::OR) ? "||" : "&&";

    if (current.isEmpty()) {
        filterField->setText(value);
    } else {
        // Avoid duplicates
        QStringList parts;
        if (current.contains("&&"))      parts = current.split("&&");
        else if (current.contains("||")) parts = current.split("||");
        else                             parts = current.split("|");

        for (const QString &part : parts) {
            if (part.trimmed() == value) {
                m_ui->statusbar->showMessage(
                    QString("'%1' is already in the filter").arg(value), 2000);
                return;
            }
        }
        filterField->setText(current + separator + value);
    }
    m_ui->statusbar->showMessage(
        QString("Added '%1' to filter (%2)").arg(value, op == FilterOperator::OR ? "OR" : "AND"),
        2000);
}

void UiManager::onLogTableDoubleClicked(const QModelIndex &index)
{
    // Route by the table that fired the click, NOT the active pane.
    // Allows marking in pane B even when pane A is active and vice versa.
    auto *pb = m_logSplitController ? m_logSplitController->paneB() : nullptr;
    const bool isPaneB = pb && pb->table && sender() == pb->table;

    QVector<LogEntry>& filtered  = isPaneB ? pb->filteredLogs : filteredLogs;
    QHash<quint64,int>& allIdx   = isPaneB ? pb->allLogsIndex : m_allLogsIndex;
    MarkLogModel*       markModel = isPaneB ? pb->markModel   : m_markLogModel;
    QSet<int>&          marked    = isPaneB ? pb->markedRows  : m_markedRows;
    LogModel*           logModel  = isPaneB ? pb->model       : m_logModel;

    if (!index.isValid() || index.row() >= filtered.size()) return;

    const LogEntry &entry = filtered[index.row()];
    const int allLogsIndex = allIdx.value(entry.id, -1);
    if (allLogsIndex < 0) return;

    if (markModel->isMarked(allLogsIndex)) {
        marked.remove(index.row());
        markModel->removeMarkedLog(allLogsIndex);
    } else {
        marked.insert(index.row());
        markModel->addMarkedLog(entry, allLogsIndex);
    }
    logModel->setMarkedRows(&marked);
}

void UiManager::onLogTableClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    Q_UNUSED(index);
}

void UiManager::onMarkLogTableClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    // Route by sender to the pane the user actually clicked in.
    auto *pb = m_logSplitController ? m_logSplitController->paneB() : nullptr;
    const bool isPaneB = pb && pb->markTable && sender() == pb->markTable;

    MarkLogModel*       markModel  = isPaneB ? pb->markModel        : m_markLogModel;
    QTableView*         logTable   = isPaneB ? pb->table             : m_ui->tableLog;
    QTableView*         markTable  = isPaneB ? pb->markTable         : m_ui->tableMarkLog;
    QVector<LogEntry>&  all        = isPaneB ? pb->allLogs           : allLogs;
    QHash<quint64,int>& filtIdx    = isPaneB ? pb->filteredLogsIndex : m_filteredLogsIndex;
    Q_UNUSED(logTable);
    Q_UNUSED(markTable);

    const int allLogsIndex = markModel->getOriginalIndex(index.row());
    if (allLogsIndex < 0 || allLogsIndex >= all.size()) return;

    auto findFiltered = [&](int idx) -> int {
        if (idx < 0 || idx >= all.size()) return -1;
        return filtIdx.value(all[idx].id, -1);
    };
    auto findNearest = [&](int idx) -> int {
        if (idx < 0 || idx >= all.size()) return -1;
        for (int i = idx + 1; i < all.size(); ++i) {
            const int row = filtIdx.value(all[i].id, -1);
            if (row >= 0) return row;
        }
        for (int i = idx - 1; i >= 0; --i) {
            const int row = filtIdx.value(all[i].id, -1);
            if (row >= 0) return row;
        }
        return -1;
    };

    int filteredRow = findFiltered(allLogsIndex);
    if (filteredRow < 0) {
        filteredRow = findNearest(allLogsIndex);
        if (filteredRow < 0) {
            m_ui->statusbar->showMessage("No visible logs found with current filters", 3000);
            return;
        }
        m_pendingCenterRow = filteredRow;
        logTable->selectRow(filteredRow);
        m_rowResizeTimer->start();
        m_ui->statusbar->showMessage(
            "Marked log is filtered out - scrolled to nearest visible log", 3000);
        return;
    }

    m_pendingCenterRow = filteredRow;
    logTable->selectRow(filteredRow);
    m_rowResizeTimer->start();
}

void UiManager::onClearAllMarkedClicked()
{
    activeMarkedRows().clear();
    activeMarkLogModel()->clear();
    activeLogModel()->setMarkedRows(&activeMarkedRows());
}

void UiManager::onMarkLogContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_ui->tableMarkLog->indexAt(pos);
    if (!idx.isValid()) return;

    const int clickedRow     = idx.row();
    const bool alreadyAnchor = (clickedRow == m_markLogModel->anchorRow());

    QMenu menu(m_mainWindow);
    // Styling now comes from qApp's themed stylesheet.

    QAction *setAnchorAction = menu.addAction(
        alreadyAnchor ? tr("\u25b6  Start time (already set)")
                      : tr("\u25b6  Set as start time (\u0394T\u00a0=\u00a00)"));
    setAnchorAction->setEnabled(!alreadyAnchor);
    menu.addSeparator();
    QAction *unmarkAction = menu.addAction(tr("\u2715  Unmark this row"));

    const QAction *chosen = menu.exec(m_ui->tableMarkLog->viewport()->mapToGlobal(pos));
    if (chosen == setAnchorAction && !alreadyAnchor) {
        m_markLogModel->setAnchorRow(clickedRow);
    } else if (chosen == unmarkAction) {
        const int allLogsIndex = m_markLogModel->getOriginalIndex(clickedRow);
        if (allLogsIndex >= 0) {
            m_markedRows.remove(findLogInFilteredLogs(allLogsIndex));
            m_markLogModel->removeMarkedLog(allLogsIndex);
            m_logModel->setMarkedRows(&m_markedRows);
        }
    }
}

void UiManager::showCellContentDialog(const QString &content, QWidget *parent)
{
    auto *dlg = new QDialog(parent ? parent : m_mainWindow);
    dlg->setWindowTitle(tr("Cell Content"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(600, 300);

    auto *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *textEdit = new QPlainTextEdit(dlg);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(content);
    textEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(textEdit);

    auto *btnClose = new QPushButton(tr("Close"), dlg);
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::close);
    layout->addWidget(btnClose, 0, Qt::AlignRight);

    // Close when the dialog loses focus (click outside) or Escape is pressed.
    auto *filter = new CloseOnDeactivateFilter(dlg);
    dlg->installEventFilter(filter);

    dlg->show();
    dlg->activateWindow();
}

void UiManager::onFitRowsClicked()
{
    auto fitAll = [](QTableView *tv) {
        if (!tv) return;
        const int rows = tv->model() ? tv->model()->rowCount() : 0;
        if (rows <= 0) return;
        // Temporarily switch the vertical header to ResizeToContents — Qt
        // computes hints for every section, then we restore Interactive so
        // the user can still drag-resize individual rows afterwards.
        auto *vh = tv->verticalHeader();
        if (!vh) return;
        const auto prev = vh->sectionResizeMode(0);
        vh->setSectionResizeMode(QHeaderView::ResizeToContents);
        QApplication::processEvents();
        vh->setSectionResizeMode(prev);
    };

    fitAll(m_ui->tableLog);
    fitAll(m_ui->tableMarkLog);
    if (m_logSplitController) {
        if (auto *pb = m_logSplitController->paneB()) {
            fitAll(pb->table);
            fitAll(pb->markTable);
        }
    }
    m_ui->statusbar->showMessage(tr("Resized rows to fit content."), 2000);
}

