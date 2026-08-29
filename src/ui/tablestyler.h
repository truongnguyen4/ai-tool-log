#ifndef TABLESTYLER_H
#define TABLESTYLER_H

#include <QAbstractItemView>
#include <QFrame>
#include <QHeaderView>
#include <initializer_list>
#include <QTableView>

// ---------------------------------------------------------------------------
// TableStyler — one place that defines how a table *looks*.
//
// The same block of "flat, no grid, per-pixel scrolling, alternating rows"
// setup used to be copy-pasted into UiManager::setupLogTable(),
// UiManager::setupConfigurationTables() and LogSplitController's pane-B
// builder, which meant pane B could silently drift from pane A. Colours and
// borders stay in the theme sheet; only structural view options live here.
// ---------------------------------------------------------------------------
namespace TableStyler {

namespace Metrics {
constexpr int kLogRowHeight        = 26;
constexpr int kLogMinRowHeight     = 22;
constexpr int kConfigRowHeight     = 28;
constexpr int kConfigMinRowHeight  = 24;
constexpr int kLogMinColumnWidth   = 48;
constexpr int kConfigMinColumnWidth = 56;
} // namespace Metrics

/** Options every table in the app shares. */
inline void applyCommonStyle(QTableView *view, int rowHeight, int minRowHeight,
                             int minColumnWidth)
{
    if (!view)
        return;

    view->setShowGrid(false);
    view->setFrameShape(QFrame::NoFrame);
    view->setAlternatingRowColors(true);
    view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    if (QHeaderView *vertical = view->verticalHeader()) {
        vertical->setVisible(false);
        vertical->setDefaultSectionSize(rowHeight);
        vertical->setMinimumSectionSize(minRowHeight);
    }
    if (QHeaderView *horizontal = view->horizontalHeader()) {
        horizontal->setHighlightSections(false);
        horizontal->setMinimumSectionSize(minColumnWidth);
    }
}

/** Log / marked-log tables: word-wrapped, multi-row selection. */
inline void applyLogTableStyle(std::initializer_list<QTableView *> views)
{
    for (QTableView *view : views) {
        if (!view)
            continue;
        applyCommonStyle(view, Metrics::kLogRowHeight, Metrics::kLogMinRowHeight,
                         Metrics::kLogMinColumnWidth);
        view->setWordWrap(true);
        view->setSortingEnabled(false);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
}

/** Configuration / SDK tables: editable cells, per-cell selection. */
inline void applyConfigTableStyle(std::initializer_list<QTableView *> views)
{
    for (QTableView *view : views) {
        if (!view)
            continue;
        applyCommonStyle(view, Metrics::kConfigRowHeight, Metrics::kConfigMinRowHeight,
                         Metrics::kConfigMinColumnWidth);
        view->setWordWrap(true);
        view->setSelectionBehavior(QAbstractItemView::SelectItems);
        if (QHeaderView *vertical = view->verticalHeader())
            vertical->setSectionResizeMode(QHeaderView::ResizeToContents);
    }
}

} // namespace TableStyler

#endif // TABLESTYLER_H
