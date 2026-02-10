#include "logmodel.h"
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
    return 7; // Time, PID, TID, Package, Lvl, Tag, Message
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_logs.size())
        return QVariant();

    const LogEntry &entry = m_logs[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return entry.time;
            case 1: return entry.pid;
            case 2: return entry.tid;
            case 3: return entry.package;
            case 4: return entry.level;
            case 5: return entry.tag;
            case 6: return entry.message;
        }
    }
    else if (role == Qt::ForegroundRole && index.column() == 4) {
        // Color code log levels
        return getLevelColor(entry.level);
    }
    else if (role == Qt::FontRole && index.column() == 4) {
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
        switch (section) {
            case 0: return "Time";
            case 1: return "PID";
            case 2: return "TID";
            case 3: return "Package";
            case 4: return "Lvl";
            case 5: return "Tag";
            case 6: return "Message";
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
