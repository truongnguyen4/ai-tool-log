#ifndef PROPERTIESMODEL_H
#define PROPERTIESMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QHash>
#include <QString>
#include <QElapsedTimer>
#include "tablequery.h"
#include "propertyentry.h"

class QTimer;

class PropertiesModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit PropertiesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void setProperties(const QVector<PropertyEntry> &properties);
    // Update values in the model.
    // allowInsert=true (default): add entries not yet present (adb fetch path).
    // allowInsert=false: update value-only for existing entries, no insertion (socket path).
    void updateProperties(const QVector<PropertyEntry> &properties, bool allowInsert = true);
    const QVector<PropertyEntry>& getProperties() const;
    // Returns the rows currently visible in the table (filtered if a filter
    // is active, otherwise all rows).
    const QVector<PropertyEntry>& visibleProperties() const;

    /** Show only the rows matching @p query (see tablequery.h); empty shows all. */
    void applyFilter(const QString &query);
    void reapplyFilter();
    void clearFilter();
    /** The filter in force, for its repair warning. */
    const TableQuery &filterQuery() const { return m_filterQuery; }

private:
    /** Recompute the visible rows from m_filterQuery and reset the view. */
    void rebuildFilter();

    QVector<PropertyEntry> m_allProperties;
    QVector<PropertyEntry> m_filteredProperties;
    TableQuery m_filterQuery;   // kept to reapply after an update
    bool m_isFiltered;

    QHash<QString, qint64> m_blinkUntil;
    QElapsedTimer          m_clock;
    QTimer                *m_blinkSweep = nullptr;
    void scheduleBlinkSweep();
};

#endif // PROPERTIESMODEL_H
