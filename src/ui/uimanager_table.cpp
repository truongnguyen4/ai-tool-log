// UiManager: log / mark-log table context menus and click handlers.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "mainwindow.h"
#include "logmodel.h"
#include "logsplitcontroller.h"
#include "marklogmodel.h"
#include "tableconfig.h"

#include <QAction>
#include <QDialog>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QClipboard>

#include "components/components.h"

namespace {

/** Dialog geometry for the "View Cell Content" popup. */
constexpr int kCellDialogWidth  = 640;
constexpr int kCellDialogHeight = 320;

/** Closes its parent window when that window loses focus. */
class CloseOnDeactivateFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::WindowDeactivate) {
            if (auto *window = qobject_cast<QWidget *>(watched))
                window->close();
        }
        // Never consume the event: the window still has to process the
        // deactivation itself, and other filters may be listening.
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Pane resolution
// ─────────────────────────────────────────────────────────────────────────────

UiManager::PaneRefs UiManager::paneRefsForSender(const QObject *senderObject)
{
    auto *paneB = m_logSplitController ? m_logSplitController->paneB() : nullptr;
    const bool isPaneB = paneB && senderObject
                         && (senderObject == paneB->table || senderObject == paneB->markTable);

    PaneRefs refs;
    refs.isPaneB      = isPaneB;
    refs.all          = isPaneB ? &paneB->allLogs           : &allLogs;
    refs.filtered     = isPaneB ? &paneB->filteredLogs      : &filteredLogs;
    refs.allIndex     = isPaneB ? &paneB->allLogsIndex      : &m_allLogsIndex;
    refs.filteredIndex= isPaneB ? &paneB->filteredLogsIndex : &m_filteredLogsIndex;
    refs.markedRows   = isPaneB ? &paneB->markedRows        : &m_markedRows;
    refs.logModel     = isPaneB ? paneB->model              : m_logModel;
    refs.markModel    = isPaneB ? paneB->markModel          : m_markLogModel;
    refs.logTable     = isPaneB ? paneB->table              : m_ui->tableLog;
    refs.markTable    = isPaneB ? paneB->markTable          : m_ui->tableMarkLog;
    return refs;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Table Interaction
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onTableContextMenu(const QPoint &pos)
{
    const PaneRefs pane = paneRefsForSender(sender());
    QTableView *table = qobject_cast<QTableView *>(sender());
    if (!table)
        table = pane.logTable;
    if (!table || !pane.logModel)
        return;

    const QModelIndex index = table->indexAt(pos);
    if (!index.isValid())
        return;

    const QString value = pane.logModel->data(index, Qt::DisplayRole).toString();

    QMenu menu(m_mainWindow);
    QAction *viewCellAction = menu.addAction(tr("View Cell Content"));

    // Filter shortcuts, for the columns a filter box exists for.
    using namespace TableConfig::LogColumns;
    QString filterField;
    QString filterLabel;
    switch (index.column()) {
    case PID:     filterField = QStringLiteral("pid");     filterLabel = tr("PID");     break;
    case TID:     filterField = QStringLiteral("tid");     filterLabel = tr("TID");     break;
    case PACKAGE: filterField = QStringLiteral("package"); filterLabel = tr("Package"); break;
    case TAG:     filterField = QStringLiteral("tag");     filterLabel = tr("Tag");     break;
    default: break;
    }

    QAction *addOrAction  = nullptr;
    QAction *addAndAction = nullptr;
    if (!filterField.isEmpty() && !value.isEmpty()) {
        menu.addSeparator();
        addOrAction  = menu.addAction(tr("Add '%1' to %2 filter (OR)").arg(value, filterLabel));
        addAndAction = menu.addAction(tr("Add '%1' to %2 filter (AND)").arg(value, filterLabel));
    }

    const QAction *chosen = menu.exec(table->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == viewCellAction)
        showCellContentDialog(value, table);
    else if (chosen == addOrAction)
        addToFilter(filterField, value, FilterOperator::OR);
    else if (chosen == addAndAction)
        addToFilter(filterField, value, FilterOperator::AND);
}

void UiManager::addToFilter(const QString &filterType, const QString &value, FilterOperator op)
{
    QLineEdit *filterField = nullptr;
    if      (filterType == QLatin1String("tag"))     filterField = m_ui->txtTagFilter;
    else if (filterType == QLatin1String("package")) filterField = m_ui->txtPackageFilter;
    else if (filterType == QLatin1String("pid"))     filterField = m_ui->txtPidFilter;
    else if (filterType == QLatin1String("tid"))     filterField = m_ui->txtTidFilter;
    if (!filterField)
        return;

    const QString current = filterField->text().trimmed();
    if (current.isEmpty()) {
        filterField->setText(value);
    } else {
        // Don't add the same term twice.
        const ParsedFilter existing = ParsedFilter::build(current);
        if (existing.parts.contains(value)) {
            flashStatus(tr("'%1' is already in the filter").arg(value));
            return;
        }
        const QLatin1String separator = (op == FilterOperator::OR) ? QLatin1String("||")
                                                                   : QLatin1String("&&");
        filterField->setText(current + separator + value);
    }

    flashStatus(tr("Added '%1' to filter (%2)")
                    .arg(value, op == FilterOperator::OR ? tr("OR") : tr("AND")));
    onFilterChanged();
}

void UiManager::onLogTableDoubleClicked(const QModelIndex &index)
{
    // Route by the table that fired the click, NOT the active pane, so the
    // user can mark rows in either pane regardless of which one has focus.
    const PaneRefs pane = paneRefsForSender(sender());
    if (!index.isValid() || index.row() >= pane.filtered->size())
        return;

    const LogEntry &entry = pane.filtered->at(index.row());
    const int allLogsIndex = pane.allIndex->value(entry.id, -1);
    if (allLogsIndex < 0)
        return;

    if (pane.markModel->isMarked(allLogsIndex)) {
        pane.markedRows->remove(index.row());
        pane.markModel->removeMarkedLog(allLogsIndex);
    } else {
        pane.markedRows->insert(index.row());
        pane.markModel->addMarkedLog(entry, allLogsIndex);
    }
    pane.logModel->setMarkedRows(pane.markedRows);
}

void UiManager::onMarkLogTableClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    const PaneRefs pane = paneRefsForSender(sender());
    const int allLogsIndex = pane.markModel->getOriginalIndex(index.row());
    if (allLogsIndex < 0 || allLogsIndex >= pane.all->size())
        return;

    int targetRow = pane.filteredIndex->value(pane.all->at(allLogsIndex).id, -1);
    bool exact = targetRow >= 0;

    if (!exact) {
        // The marked entry is filtered out; walk outward for the closest row
        // that is still visible so the click still takes the user somewhere.
        for (int i = allLogsIndex + 1; targetRow < 0 && i < pane.all->size(); ++i)
            targetRow = pane.filteredIndex->value(pane.all->at(i).id, -1);
        for (int i = allLogsIndex - 1; targetRow < 0 && i >= 0; --i)
            targetRow = pane.filteredIndex->value(pane.all->at(i).id, -1);
    }

    if (targetRow < 0) {
        flashStatus(tr("No visible logs found with current filters"));
        return;
    }

    m_pendingCenterRow = targetRow;
    if (pane.logTable)
        pane.logTable->selectRow(targetRow);
    m_rowResizeTimer->start();

    if (!exact)
        flashStatus(tr("Marked log is filtered out — scrolled to nearest visible log"));
}

void UiManager::onClearAllMarkedClicked()
{
    activeMarkedRows().clear();
    activeMarkLogModel()->clear();
    activeLogModel()->setMarkedRows(&activeMarkedRows());
}

void UiManager::onMarkLogContextMenu(const QPoint &pos)
{
    const PaneRefs pane = paneRefsForSender(sender());
    QTableView *table = qobject_cast<QTableView *>(sender());
    if (!table)
        table = pane.markTable;
    if (!table || !pane.markModel)
        return;

    const QModelIndex index = table->indexAt(pos);
    if (!index.isValid())
        return;

    const int clickedRow     = index.row();
    const bool alreadyAnchor = clickedRow == pane.markModel->anchorRow();

    QMenu menu(m_mainWindow);
    QAction *setAnchorAction = menu.addAction(
        alreadyAnchor ? tr("▶  Start time (already set)")
                      : tr("▶  Set as start time (ΔT = 0)"));
    setAnchorAction->setEnabled(!alreadyAnchor);
    menu.addSeparator();
    QAction *unmarkAction = menu.addAction(tr("✕  Unmark this row"));

    const QAction *chosen = menu.exec(table->viewport()->mapToGlobal(pos));
    if (chosen == setAnchorAction && !alreadyAnchor) {
        pane.markModel->setAnchorRow(clickedRow);
        return;
    }
    if (chosen != unmarkAction)
        return;

    const int allLogsIndex = pane.markModel->getOriginalIndex(clickedRow);
    if (allLogsIndex < 0 || allLogsIndex >= pane.all->size())
        return;

    const int filteredRow = pane.filteredIndex->value(pane.all->at(allLogsIndex).id, -1);
    if (filteredRow >= 0)
        pane.markedRows->remove(filteredRow);
    pane.markModel->removeMarkedLog(allLogsIndex);
    pane.logModel->setMarkedRows(pane.markedRows);
}

void UiManager::showCellContentDialog(const QString &content, QWidget *parent)
{
    auto *dialog = new QDialog(parent ? parent : m_mainWindow);
    dialog->setWindowTitle(tr("Cell Content"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(kCellDialogWidth, kCellDialogHeight);

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *textEdit = new QPlainTextEdit(dialog);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(content);
    textEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    layout->addWidget(textEdit);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    auto *copyButton = UiComponents::Button::make(tr("Copy"),
                                                  UiComponents::ButtonVariant::Secondary,
                                                  dialog, UiComponents::ButtonSize::Small);
    auto *closeButton = UiComponents::Button::make(tr("Close"),
                                                   UiComponents::ButtonVariant::Primary,
                                                   dialog, UiComponents::ButtonSize::Small);
    buttonRow->addWidget(copyButton);
    buttonRow->addWidget(closeButton);
    layout->addLayout(buttonRow);

    connect(copyButton, &QPushButton::clicked, dialog, [this, content]() {
        QApplication::clipboard()->setText(content);
        flashStatus(tr("Cell content copied to clipboard"));
    });
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);

    // Escape closes, and so does clicking outside the dialog.
    auto *closeShortcut = new QAction(dialog);
    closeShortcut->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(closeShortcut, &QAction::triggered, dialog, &QDialog::close);
    dialog->addAction(closeShortcut);
    dialog->installEventFilter(new CloseOnDeactivateFilter(dialog));

    dialog->show();
    dialog->activateWindow();
}

void UiManager::onFitRowsClicked()
{
    // Ask the vertical header for a one-off content fit. This is O(rows) with
    // a layout pass per row, so it stays an explicit user action rather than
    // something the app does on its own while logs are streaming in.
    const auto fitAll = [](QTableView *view) {
        if (!view || !view->model() || view->model()->rowCount() <= 0)
            return;
        QHeaderView *header = view->verticalHeader();
        if (!header)
            return;
        const QHeaderView::ResizeMode previous = header->sectionResizeMode(0);
        header->setSectionResizeMode(QHeaderView::ResizeToContents);
        header->resizeSections(QHeaderView::ResizeToContents);
        header->setSectionResizeMode(previous);
    };

    fitAll(m_ui->tableLog);
    fitAll(m_ui->tableMarkLog);
    if (m_logSplitController) {
        if (auto *paneB = m_logSplitController->paneB()) {
            fitAll(paneB->table);
            fitAll(paneB->markTable);
        }
    }
    flashStatus(tr("Resized rows to fit content."));
}
