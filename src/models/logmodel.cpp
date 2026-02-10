#include "logmodel.h"
#include "tableconfig.h"
#include <QFont>

LogModel::LogModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_markedRows(nullptr)
{}

int LogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_logs.size();
}

int LogModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return TableConfig::LogColumns::TOTAL_COLUMNS;
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_logs.size())
        return QVariant();

    const LogEntry &entry = m_logs[index.row()];

    if (role == Qt::DisplayRole) {
        using namespace TableConfig::LogColumns;
        switch (index.column()) {
            case DATE:    return entry.date;
            case TIME:    return entry.time;
            case PID:     return entry.pid;
            case TID:     return entry.tid;
            case PACKAGE: return entry.package;
            case LEVEL:   return entry.level;
            case TAG:     return entry.tag;
            case MESSAGE: return entry.message;
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        if (index.column() == TableConfig::LogColumns::DATE)
            return Qt::AlignCenter;
        // All other columns: left + vertically centered
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    else if (role == Qt::ForegroundRole) {
        return getLevelColor(entry.level);
    }
    else if (role == Qt::FontRole && index.column() == TableConfig::LogColumns::LEVEL) {
        // Bold font for log level
        QFont font;
        font.setBold(true);
        return font;
    }
    else if (role == Qt::BackgroundRole) {
        // Highlight marked rows
        if (m_markedRows && m_markedRows->contains(index.row())) {
            return QColor("#30567a"); // Slightly lighter than normal for marked rows
        }
    }

    return QVariant();
}

QVariant LogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        using namespace TableConfig::LogColumns;
        switch (section) {
            case DATE:    return "Date";
            case TIME:    return "Time";
            case PID:     return "PID";
            case TID:     return "TID";
            case PACKAGE: return "Package";
            case LEVEL:   return "Lvl";
            case TAG:     return "Tag";
            case MESSAGE: return "Message";
        }
    }
    else {
        return section + 1;
    }

    return QVariant();
}

void LogModel::setLogs(const QVector<LogEntry> &logs)
{
    beginResetModel();
    m_logs = logs;
    endResetModel();
}

void LogModel::addLog(const LogEntry &entry)
{
    int row = m_logs.size();
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
    m_logs.reserve(m_logs.size() + entries.size());
    for (const LogEntry &e : entries)
        m_logs.append(e);
    endInsertRows();
}

void LogModel::clear()
{
    beginResetModel();
    m_logs.clear();
    endResetModel();
}

const LogEntry& LogModel::getLogEntry(int row) const
{
    return m_logs[row];
}

int LogModel::getLogCount() const
{
    return m_logs.size();
}

void LogModel::setMarkedRows(const QSet<int> *markedRows)
{
    m_markedRows = markedRows;
    // Trigger repaint of all rows
    if (!m_logs.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_logs.size() - 1, columnCount() - 1));
    }
}

QColor LogModel::getLevelColor(const QString &level) const
{
    if (level == "V") return QColor("#9ca3af"); // Verbose - Gray
    if (level == "D") return QColor("#60a5fa"); // Debug - Blue
    if (level == "I") return QColor("#34d399"); // Info - Green
    if (level == "W") return QColor("#fbbf24"); // Warn - Yellow
    if (level == "E") return QColor("#f87171"); // Error - Red
    if (level == "A") return QColor("#c084fc"); // Assert - Purple
    return QColor("#CCCCCC");
}
