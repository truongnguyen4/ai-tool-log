#include "marklogmodel.h"
#include "tableconfig.h"
#include <QFont>
#include <QDateTime>

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
    return 9; // Date, Time, PID, TID, Package, Lvl, Tag, Message, DeltaTime
}

QVariant MarkLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_markedLogs.size())
        return QVariant();

    const LogEntry &entry = m_markedLogs[index.row()].entry;

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
            case DELTA: {
                // Anchor row is T=0 — show em-dash
                if (index.row() == m_anchorRow)
                    return QString("\u2014");
                // All other rows: signed delta relative to the anchor row's timestamp
                const LogEntry &anchor = m_markedLogs[m_anchorRow].entry;
                const QDateTime curDT = QDateTime::fromString(
                    entry.date + " " + entry.time, "yyyy-MM-dd HH:mm:ss.zzz");
                const QDateTime anchorDT = QDateTime::fromString(
                    anchor.date + " " + anchor.time, "yyyy-MM-dd HH:mm:ss.zzz");
                if (!curDT.isValid() || !anchorDT.isValid())
                    return QString("\u2014");
                const qint64 ms = anchorDT.msecsTo(curDT);
                if (ms == 0) return QString("0 ms");
                return QString("%1%2 ms").arg(ms > 0 ? "+" : "").arg(ms);
            }
        }
    }
    else if (role == Qt::TextAlignmentRole && index.column() == TableConfig::LogColumns::DATE) {
        // Center align the Date column
        return Qt::AlignCenter;
    }
    else if (role == Qt::BackgroundRole) {
        // Highlight the anchor row with a subtle teal tint
        if (index.row() == m_anchorRow)
            return QColor("#1a3a4a"); // dark teal — marks the T=0 anchor
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

    return QVariant();
}

QVariant MarkLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        using namespace TableConfig::LogColumns;
        switch (section) {
            case DATE:    return Names::DATE;
            case TIME:    return Names::TIME;
            case PID:     return Names::PID;
            case TID:     return Names::TID;
            case PACKAGE: return Names::PACKAGE;
            case LEVEL:   return Names::LEVEL;
            case TAG:     return Names::TAG;
            case MESSAGE: return Names::MESSAGE;
            case DELTA:   return Names::DELTA;
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
            // Adjust anchor: if we removed a row before the anchor, shift it back;
            // if we removed the anchor itself, reset to row 0.
            if (i < m_anchorRow)
                m_anchorRow--;
            else if (i == m_anchorRow)
                m_anchorRow = 0;
            // Rows after anchor are unaffected.
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
    m_anchorRow = 0;
    endResetModel();
}

int MarkLogModel::getMarkedCount() const
{
    return m_markedLogs.size();
}

void MarkLogModel::setAnchorRow(int row)
{
    if (row < 0 || row >= m_markedLogs.size() || row == m_anchorRow)
        return;
    m_anchorRow = row;
    // Force every cell in the table to repaint:
    //  - Qt::DisplayRole  → recalculate the DELTA text for every row
    //  - Qt::BackgroundRole → move the teal anchor-row highlight
    const QModelIndex topLeft     = index(0, 0);
    const QModelIndex bottomRight = index(m_markedLogs.size() - 1, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole, Qt::BackgroundRole});
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
