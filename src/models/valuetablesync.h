#ifndef VALUETABLESYNC_H
#define VALUETABLESYNC_H

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

// ---------------------------------------------------------------------------
// ValueTableSync — key-indexed merge helpers for "name + value" table models
// (settings, system properties, ...).
//
// SettingsModel and PropertiesModel both used to merge incoming device reads
// with a nested loop over every existing row. With a few thousand settings and
// a monitor ticking every 250 ms that is millions of string comparisons per
// second. These helpers build a key -> row index once and merge in O(n + m).
//
// Entries only need a public `value` and `line` member; identity is supplied
// by the caller so each model keeps its own matching rules.
// ---------------------------------------------------------------------------
namespace ValueTableSync {

struct MergeResult {
    bool          changed = false;  ///< any row was updated or inserted
    QSet<QString> changedKeys;      ///< primary keys whose value actually changed
};

/**
 * Row lookup keyed by one or more aliases per row.
 *
 * Aliases let a caller match a partially-specified incoming row — for example
 * a settings push that names the setting but not its namespace. The first row
 * to claim an alias keeps it, so aliases never hide an exact match.
 */
class RowIndex
{
public:
    void reserve(int rows) { m_rows.reserve(rows); }

    void addAliases(const QStringList &aliases, int row)
    {
        for (const QString &alias : aliases) {
            if (!alias.isEmpty() && !m_rows.contains(alias))
                m_rows.insert(alias, row);
        }
    }

    /** Row for the first alias present in the index, or -1. */
    int find(const QStringList &aliases) const
    {
        for (const QString &alias : aliases) {
            const auto it = m_rows.constFind(alias);
            if (it != m_rows.constEnd())
                return it.value();
        }
        return -1;
    }

private:
    QHash<QString, int> m_rows;
};

/**
 * Merge @p incoming into @p target.
 *
 * @param allowInsert    append rows that have no match (full-fetch path);
 *                       when false only existing values are refreshed.
 * @param aliasesOf      identity aliases of a row; aliasesOf(e).first() is the
 *                       primary key reported in MergeResult::changedKeys.
 */
template <typename Entry, typename AliasFn>
MergeResult merge(QVector<Entry> &target,
                  const QVector<Entry> &incoming,
                  bool allowInsert,
                  AliasFn aliasesOf)
{
    MergeResult result;

    RowIndex index;
    index.reserve(target.size());
    for (int row = 0; row < target.size(); ++row)
        index.addAliases(aliasesOf(target.at(row)), row);

    for (const Entry &in : incoming) {
        const QStringList aliases = aliasesOf(in);
        const int row = index.find(aliases);

        if (row >= 0) {
            Entry &existing = target[row];
            if (existing.value != in.value) {
                result.changedKeys.insert(aliasesOf(existing).value(0));
                existing.value = in.value;
                result.changed = true;
            }
            if (allowInsert)
                existing.line = in.line;
        } else if (allowInsert) {
            index.addAliases(aliases, target.size());
            target.append(in);
            result.changed = true;
        }
    }

    return result;
}

/** Refresh @p target values from @p source rows sharing a key. O(n + m). */
template <typename Entry, typename AliasFn>
void copyValues(QVector<Entry> &target, const QVector<Entry> &source, AliasFn aliasesOf)
{
    if (target.isEmpty() || source.isEmpty())
        return;

    RowIndex index;
    index.reserve(source.size());
    for (int row = 0; row < source.size(); ++row)
        index.addAliases(aliasesOf(source.at(row)), row);

    for (Entry &entry : target) {
        const int row = index.find(aliasesOf(entry));
        if (row >= 0)
            entry.value = source.at(row).value;
    }
}

} // namespace ValueTableSync

#endif // VALUETABLESYNC_H
