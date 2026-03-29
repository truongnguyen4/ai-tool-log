#ifndef PROPERTIESMODEL_H
#define PROPERTIESMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include "iconfigfilter.h"
#include "configfilter.h"
#include "propertyentry.h"

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
    
    void applyFilter(const QString &nameFilter, const QString &valueFilter = QString());
    void reapplyFilter();
    void clearFilter();

private:
    QVector<PropertyEntry> m_allProperties;
    QVector<PropertyEntry> m_filteredProperties;
    ConfigFilter m_filter;
    bool m_isFiltered;
    QString m_currentNameFilter;   // Store current name filter to reapply after update
    QString m_currentValueFilter;  // Store current value filter to reapply after update
};

#endif // PROPERTIESMODEL_H
