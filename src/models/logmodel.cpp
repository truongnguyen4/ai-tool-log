#include "logmodel.h"

#include "colorscheme.h"
#include "tableconfig.h"

LogModel::LogModel(QObject *parent)
    : LogTableModelBase(parent)
{
}

int LogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_logs.size();
}

int LogModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : TableConfig::LogColumns::TOTAL_COLUMNS;
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_logs.size())
        return {};

    if (role == Qt::BackgroundRole) {
        return (m_markedRows && m_markedRows->contains(index.row()))
                   ? QVariant(ColorScheme::instance().markedRowBackground())
                   : QVariant();
    }

    return entryData(m_logs.at(index.row()), index.column(), role);
}

void LogModel::setLogs(const QVector<LogEntry> &logs)
{
    beginResetModel();
    m_logs = logs;
    endResetModel();
}

void LogModel::addLog(const LogEntry &entry)
{
    const int row = m_logs.size();
    beginInsertRows(QModelIndex(), row, row);
    m_logs.append(entry);
    endInsertRows();
}

void LogModel::addLogs(const QVector<LogEntry> &entries)
{
    if (entries.isEmpty())
        return;

    const int first = m_logs.size();
    const int last  = first + entries.size() - 1;
    beginInsertRows(QModelIndex(), first, last);
    m_logs.reserve(first + entries.size());
    m_logs.append(entries);
    endInsertRows();
}

void LogModel::clear()
{
    beginResetModel();
    m_logs.clear();
    endResetModel();
}

const LogEntry &LogModel::getLogEntry(int row) const
{
    static const LogEntry kEmpty;
    if (row < 0 || row >= m_logs.size())
        return kEmpty;
    return m_logs.at(row);
}

int LogModel::getLogCount() const
{
    return m_logs.size();
}

void LogModel::setMarkedRows(const QSet<int> *markedRows)
{
    m_markedRows = markedRows;
    refreshAllRows({Qt::BackgroundRole});
}
