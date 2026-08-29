#include "marklogmodel.h"

#include "colorscheme.h"
#include "tableconfig.h"

#include <QDateTime>

#include <algorithm>

namespace {

constexpr auto kTimestampForm = "yyyy-MM-dd HH:mm:ss.zzz";

/** Placeholder shown when a delta cannot be computed (em dash). */
QString kEmDash() { return QStringLiteral("\u2014"); }

/** Chronological key for sorting/comparing marked rows. */
QString timestampKey(const LogEntry &e)
{
    return e.date + QLatin1Char(' ') + e.time;
}
} // namespace

MarkLogModel::MarkLogModel(QObject *parent)
    : LogTableModelBase(parent)
{
}

int MarkLogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_markedLogs.size();
}

int MarkLogModel::columnCount(const QModelIndex &parent) const
{
    // Same columns as the log table plus the trailing ΔTime column.
    return parent.isValid() ? 0 : TableConfig::LogColumns::DELTA + 1;
}

QVariant MarkLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_markedLogs.size())
        return {};

    const int row = index.row();

    if (role == Qt::BackgroundRole) {
        return row == m_anchorRow
                   ? QVariant(ColorScheme::instance().anchorRowBackground())
                   : QVariant();
    }
    if (role == Qt::DisplayRole && index.column() == TableConfig::LogColumns::DELTA)
        return deltaText(row);

    return entryData(m_markedLogs.at(row).entry, index.column(), role);
}

QString MarkLogModel::deltaText(int row) const
{
    if (row == m_anchorRow || m_anchorRow < 0 || m_anchorRow >= m_markedLogs.size())
        return kEmDash();

    const QDateTime current = QDateTime::fromString(
        timestampKey(m_markedLogs.at(row).entry), QLatin1String(kTimestampForm));
    const QDateTime anchor = QDateTime::fromString(
        timestampKey(m_markedLogs.at(m_anchorRow).entry), QLatin1String(kTimestampForm));
    if (!current.isValid() || !anchor.isValid())
        return kEmDash();

    const qint64 ms = anchor.msecsTo(current);
    if (ms == 0)
        return QStringLiteral("0 ms");
    return QStringLiteral("%1%2 ms").arg(ms > 0 ? QStringLiteral("+") : QString()).arg(ms);
}

void MarkLogModel::addMarkedLog(const LogEntry &entry, int originalIndex)
{
    if (isMarked(originalIndex))
        return;

    // Keep rows chronological. Comparing date+time (not time alone) keeps
    // captures that span midnight in the right order.
    const QString key = timestampKey(entry);
    const auto pos = std::lower_bound(
        m_markedLogs.cbegin(), m_markedLogs.cend(), key,
        [](const MarkedLogEntry &lhs, const QString &rhs) {
            return timestampKey(lhs.entry) < rhs;
        });
    const int insertPos = static_cast<int>(pos - m_markedLogs.cbegin());

    beginInsertRows(QModelIndex(), insertPos, insertPos);
    m_markedLogs.insert(insertPos, MarkedLogEntry{entry, originalIndex});
    // Inserting at or above the anchor shifts it down by one row.
    if (!m_markedLogs.isEmpty() && insertPos <= m_anchorRow && m_markedLogs.size() > 1)
        ++m_anchorRow;
    endInsertRows();

    // Every ΔTime value is relative to the anchor, so they all move with it.
    refreshColumn(TableConfig::LogColumns::DELTA, {Qt::DisplayRole});
}

void MarkLogModel::removeMarkedLog(int originalIndex)
{
    for (int i = 0; i < m_markedLogs.size(); ++i) {
        if (m_markedLogs.at(i).originalIndex != originalIndex)
            continue;

        beginRemoveRows(QModelIndex(), i, i);
        m_markedLogs.removeAt(i);
        // Adjust the anchor *inside* the removal so views never observe an
        // anchor index pointing past the end of the list.
        if (i < m_anchorRow)
            --m_anchorRow;
        else if (i == m_anchorRow)
            m_anchorRow = 0;
        endRemoveRows();

        refreshColumn(TableConfig::LogColumns::DELTA, {Qt::DisplayRole});
        return;
    }
}

bool MarkLogModel::isMarked(int originalIndex) const
{
    return std::any_of(m_markedLogs.cbegin(), m_markedLogs.cend(),
                       [originalIndex](const MarkedLogEntry &m) {
                           return m.originalIndex == originalIndex;
                       });
}

int MarkLogModel::getOriginalIndex(int row) const
{
    if (row < 0 || row >= m_markedLogs.size())
        return -1;
    return m_markedLogs.at(row).originalIndex;
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
    // DisplayRole  → recompute every ΔTime; BackgroundRole → move the tint.
    refreshAllRows({Qt::DisplayRole, Qt::BackgroundRole});
}
