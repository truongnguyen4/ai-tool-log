#ifndef LOGMODEL_H
#define LOGMODEL_H

#include <QSet>
#include <QVector>

#include "logtablemodelbase.h"

/**
 * Table model over the currently visible (filtered) log entries.
 *
 * The marked-row set is *not* owned: UiManager keeps one per pane and hands
 * the model a pointer so both stay in sync without copying.
 */
class LogModel : public LogTableModelBase
{
    Q_OBJECT

public:
    explicit LogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setLogs(const QVector<LogEntry> &logs);
    void addLog(const LogEntry &entry);
    void addLogs(const QVector<LogEntry> &entries);  ///< batch insert — one signal per flush
    void clear();

    /** Entry at @p row, or a default-constructed entry when out of range. */
    const LogEntry &getLogEntry(int row) const;
    int getLogCount() const;

    /** Point the model at the caller-owned set of marked (visible) rows. */
    void setMarkedRows(const QSet<int> *markedRows);

private:
    QVector<LogEntry> m_logs;
    const QSet<int>  *m_markedRows = nullptr;
};

#endif // LOGMODEL_H
