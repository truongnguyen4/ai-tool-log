#ifndef MARKLOGMODEL_H
#define MARKLOGMODEL_H

#include <QVector>

#include "logtablemodelbase.h"

/** A log entry the user bookmarked, plus its index in the unfiltered log. */
struct MarkedLogEntry {
    LogEntry entry;
    int      originalIndex = -1;
};

/**
 * Table model for bookmarked log entries.
 *
 * Adds a ΔTime column that shows each row's offset from the *anchor* row
 * (the user-chosen T=0 reference). Rows are kept sorted by timestamp.
 */
class MarkLogModel : public LogTableModelBase
{
    Q_OBJECT

public:
    explicit MarkLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void addMarkedLog(const LogEntry &entry, int originalIndex);
    void removeMarkedLog(int originalIndex);
    bool isMarked(int originalIndex) const;
    int  getOriginalIndex(int row) const;
    void clear();
    int  getMarkedCount() const;

    /** Row whose timestamp is T=0 for the ΔTime column. */
    void setAnchorRow(int row);
    int  anchorRow() const { return m_anchorRow; }

private:
    /** ΔTime text for @p row relative to the anchor row. */
    QString deltaText(int row) const;

    QVector<MarkedLogEntry> m_markedLogs;
    int                     m_anchorRow = 0;
};

#endif // MARKLOGMODEL_H
