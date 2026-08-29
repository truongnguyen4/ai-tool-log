#ifndef PROPERTYDEFINITIONMODEL_H
#define PROPERTYDEFINITIONMODEL_H

#include <QAbstractTableModel>
#include <QElapsedTimer>
#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>

#include "propertydefinition.h"
#include "propertydefinitionquery.h"

class QTimer;

/**
 * Every configuration_manager property of the current device, with the
 * changes the user has staged but not yet written.
 *
 * Editing a value never writes to the device by itself: it stages the new
 * value, which the table shows in place of the device's until the change is
 * applied or discarded. A refresh from the device keeps staged changes, and
 * drops the ones the device now already holds.
 *
 * Rows are the properties that pass the filter box and the scope selector,
 * in the order of the sorted column.
 */
class PropertyDefinitionModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    /** Which properties the table shows, before the filter box narrows it. */
    enum class Scope {
        All,
        Modified,     ///< value differs from the default, or a change is staged
        Pending,      ///< a change is staged
        Writable,
        ReadOnly,
        NeedsReboot,
        Unavailable,
    };

    explicit PropertyDefinitionModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // ── Catalog ──────────────────────────────────────────────────────────────
    /**
     * Replace the catalog. When it lists the same properties as before — a
     * refresh — rows stay in place and changed values blink.
     */
    void setDefinitions(const QVector<PropertyDefinition> &definitions);
    void clear();
    const QVector<PropertyDefinition> &definitions() const { return m_definitions; }
    const PropertyDefinition *definitionAt(int row) const;
    const PropertyDefinition *find(const QString &name) const;
    const PropertyDefinition *findById(int id) const;
    /** Table row of @p name, or -1 when it is filtered out. */
    int rowOf(const QString &name) const;

    int modifiedCount() const;
    int unavailableCount() const;

    // ── What is shown ────────────────────────────────────────────────────────
    void setFilter(const QString &text);
    const PropertyDefinitionQuery &filterQuery() const { return m_query; }
    void setScope(Scope scope);
    Scope scope() const { return m_scope; }

    // ── Staged changes ───────────────────────────────────────────────────────
    /** The staged value of @p def when there is one, else its device value. */
    QString shownValue(const PropertyDefinition &def) const;

    /**
     * Stage @p value for @p name. Staging the value the device already holds
     * drops the staged change. Returns why the value was refused, or an empty
     * string.
     */
    QString stage(const QString &name, const QString &value);
    void unstage(const QStringList &names);
    void discardAll();
    /**
     * The device confirmed these values were written: they become the device
     * values, and a staged change is done when it is the value written. One
     * edited again while the write ran stays staged.
     */
    void markApplied(const QVector<QPair<QString, QString>> &written);
    /** Keep a staged change and note why writing it failed. */
    void setWriteError(const QString &name, const QString &error);

    bool isPending(const QString &name) const { return m_pending.contains(name); }
    int pendingCount() const { return int(m_pending.size()); }
    /**
     * Staged changes as name / value pairs, in catalog order. Without
     * @p includeFailed, changes whose last write failed are left out.
     */
    QVector<QPair<QString, QString>> pendingChanges(bool includeFailed = true) const;

signals:
    void pendingChanged(int count);

private:
    struct Pending {
        QString value;
        QString error;   ///< why the last write of this value failed
    };

    bool passes(const PropertyDefinition &def) const;
    /** Catalog indexes of the rows to show, filtered and sorted. */
    QVector<int> visibleIndexes() const;
    void sortIndexes(QVector<int> &indexes) const;
    /** Recompute the rows; a reset only when they actually change. */
    void refreshRows(bool forceReset);
    void rebuildRowLookup();
    void emitRowChanged(const QString &name);
    QString tooltip(const PropertyDefinition &def) const;
    QString notes(const PropertyDefinition &def) const;
    void scheduleBlinkSweep();

    QVector<PropertyDefinition> m_definitions;
    QHash<QString, int>         m_indexByName;
    QHash<int, int>             m_indexById;
    QVector<int>                m_rows;         ///< table row -> catalog index
    QHash<int, int>             m_rowByIndex;   ///< catalog index -> table row
    QHash<QString, Pending>     m_pending;

    PropertyDefinitionQuery m_query;
    Scope                   m_scope = Scope::All;
    int                     m_sortColumn = -1;  ///< -1: catalog order
    Qt::SortOrder           m_sortOrder  = Qt::AscendingOrder;

    QHash<QString, qint64> m_blinkUntil;   ///< property name -> deadline ms
    QElapsedTimer          m_clock;
    QTimer                *m_blinkSweep = nullptr;
};

#endif // PROPERTYDEFINITIONMODEL_H
