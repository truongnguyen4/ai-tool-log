// UiManager: log/mark-log table copy-to-clipboard helpers.
#include "uimanager.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>

void UiManager::copyTableRows(QTableView *tableView)
{
    QAbstractItemModel *model = tableView->model();
    if (!model) return;

    QItemSelectionModel *selModel = tableView->selectionModel();

    // Try full-row selection first (log table uses SelectRows)
    QModelIndexList selectedRows = selModel->selectedRows();
    if (!selectedRows.isEmpty()) {
        std::sort(selectedRows.begin(), selectedRows.end(),
                  [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });

        QList<int> visibleCols;
        for (int c = 0; c < model->columnCount(); ++c)
            if (!tableView->isColumnHidden(c)) visibleCols.append(c);

        QStringList lines;
        for (const QModelIndex &rowIdx : selectedRows) {
            QStringList cells;
            for (int col : visibleCols)
                cells << model->data(model->index(rowIdx.row(), col), Qt::DisplayRole).toString();
            lines << cells.join(QStringLiteral("\t"));
        }
        QApplication::clipboard()->setText(lines.join(QStringLiteral("\n")));
        m_ui->statusbar->showMessage(
            QString("Copied %1 row(s) to clipboard").arg(selectedRows.size()), 2000);
        return;
    }

    // Cell-level selection (settings / properties / property-definitions tables)
    QModelIndexList selected = selModel->selectedIndexes();
    if (selected.isEmpty()) {
        QModelIndex current = tableView->currentIndex();
        if (!current.isValid()) return;
        selected.append(current);
    }

    std::sort(selected.begin(), selected.end(),
              [](const QModelIndex &a, const QModelIndex &b) {
                  return a.row() != b.row() ? a.row() < b.row() : a.column() < b.column();
              });

    // Group by row, emit tab-separated cells per row
    QStringList lines;
    int prevRow = -1;
    QStringList rowCells;
    for (const QModelIndex &idx : selected) {
        if (idx.row() != prevRow) {
            if (!rowCells.isEmpty())
                lines << rowCells.join(QStringLiteral("\t"));
            rowCells.clear();
            prevRow = idx.row();
        }
        rowCells << model->data(idx, Qt::DisplayRole).toString();
    }
    if (!rowCells.isEmpty())
        lines << rowCells.join(QStringLiteral("\t"));

    QApplication::clipboard()->setText(lines.join(QStringLiteral("\n")));
    m_ui->statusbar->showMessage(
        QString("Copied %1 cell(s) to clipboard").arg(selected.size()), 2000);
}

void UiManager::enableTableCopyAction(QTableView *tableView)
{
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QAction *copyAction = new QAction(tableView);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(copyAction, &QAction::triggered, this, [this, tableView]() {
        copyTableRows(tableView);
    });
    tableView->addAction(copyAction);
}
