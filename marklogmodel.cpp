#include "marklogmodel.h"
#include <QFont>

MarkLogModel::MarkLogModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int MarkLogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_markedLogs.size();
}

int MarkLogModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 8; // Date, Time, PID, TID, Package, Lvl, Tag, Message
}

QVariant MarkLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_markedLogs.size())
        return QVariant();

    const LogEntry &entry = m_markedLogs[index.row()].entry;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return entry.date;
            case 1: return entry.time;
            case 2: return entry.pid;
            case 3: return entry.tid;
            case 4: return entry.package;
            case 5: return entry.level;
            case 6: return entry.tag;
            case 7: return entry.message;
        }
    }
    else if (role == Qt::ForegroundRole && index.column() == 5) {
        // Color code log levels
        return getLevelColor(entry.level);
    }
    else if (role == Qt::FontRole && index.column() == 5) {
        // Bold font for log level
        QFont font;
        font.setBold(true);
        return font;
    }

    return QVariant();
}

QVariant MarkLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return "Date";
            case 1: return "Time";
            case 2: return "PID";
            case 3: return "TID";
            case 4: return "Package";
            case 5: return "Lvl";
            case 6: return "Tag";
            case 7: return "Message";
        }
    }
    else {
        return section + 1;
    }

    return QVariant();
}

void MarkLogModel::addMarkedLog(const LogEntry &entry, int originalIndex)
{
    // Check if already marked
    for (const MarkedLogEntry &marked : m_markedLogs) {
        if (marked.originalIndex == originalIndex) {
            return; // Already marked
        }
    }

    MarkedLogEntry markedEntry;
    markedEntry.entry = entry;
    markedEntry.originalIndex = originalIndex;
    
    // Find the correct position to insert based on time (sorted order)
    int insertPos = 0;
    for (int i = 0; i < m_markedLogs.size(); ++i) {
        if (entry.time < m_markedLogs[i].entry.time) {
            insertPos = i;
            break;
        }
        insertPos = i + 1;
    }
    
    beginInsertRows(QModelIndex(), insertPos, insertPos);
    m_markedLogs.insert(insertPos, markedEntry);
    endInsertRows();
}

void MarkLogModel::removeMarkedLog(int originalIndex)
{
    for (int i = 0; i < m_markedLogs.size(); ++i) {
        if (m_markedLogs[i].originalIndex == originalIndex) {
            beginRemoveRows(QModelIndex(), i, i);
            m_markedLogs.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

bool MarkLogModel::isMarked(int originalIndex) const
{
    for (const MarkedLogEntry &marked : m_markedLogs) {
        if (marked.originalIndex == originalIndex) {
            return true;
        }
    }
    return false;
}

int MarkLogModel::getOriginalIndex(int row) const
{
    if (row >= 0 && row < m_markedLogs.size()) {
        return m_markedLogs[row].originalIndex;
    }
    return -1;
}

void MarkLogModel::clear()
{
    beginResetModel();
    m_markedLogs.clear();
    endResetModel();
}

int MarkLogModel::getMarkedCount() const
{
    return m_markedLogs.size();
}

QColor MarkLogModel::getLevelColor(const QString &level) const
{
    if (level == "V") return QColor("#9ca3af");      // Gray
    else if (level == "D") return QColor("#60a5fa"); // Blue
    else if (level == "I") return QColor("#34d399"); // Green
    else if (level == "W") return QColor("#fbbf24"); // Yellow
    else if (level == "E") return QColor("#f87171"); // Red
    else if (level == "A") return QColor("#c084fc"); // Purple
    return QColor("#cccccc");
}
