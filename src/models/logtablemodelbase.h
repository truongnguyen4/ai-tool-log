#ifndef LOGTABLEMODELBASE_H
#define LOGTABLEMODELBASE_H

#include <QAbstractTableModel>
#include <QList>

#include "logentry.h"

// ---------------------------------------------------------------------------
// LogTableModelBase — the rendering rules shared by every table that shows
// LogEntry rows (the live log table and the marked-log table).
//
// Both models used to carry byte-identical DisplayRole / headerData /
// ForegroundRole / FontRole / TextAlignmentRole switches. Those now live here
// once; subclasses only supply their row storage and any extra column (e.g.
// MarkLogModel's ΔTime).
//
// Subclasses must implement rowCount()/columnCount() and route data() through
// entryData() for the shared columns.
// ---------------------------------------------------------------------------
class LogTableModelBase : public QAbstractTableModel
{
    Q_OBJECT

public:
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

protected:
    explicit LogTableModelBase(QObject *parent = nullptr);

    /**
     * Shared per-cell rendering for the LogEntry columns.
     * Returns an invalid QVariant when this (column, role) pair is not one of
     * the shared ones, letting the subclass handle it.
     */
    QVariant entryData(const LogEntry &entry, int column, int role) const;

    /** Emit dataChanged over the whole table for the given roles. */
    void refreshAllRows(const QList<int> &roles = {});

    /** Emit dataChanged over a single column for every row. */
    void refreshColumn(int column, const QList<int> &roles = {});
};

#endif // LOGTABLEMODELBASE_H
