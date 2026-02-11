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
    const QVector<PropertyEntry>& getProperties() const;
    
    void applyFilter(const QString &filterText);
    void clearFilter();

private:
    QVector<PropertyEntry> m_allProperties;
    QVector<PropertyEntry> m_filteredProperties;
    ConfigFilter m_filter;
    bool m_isFiltered;
};

#endif // PROPERTIESMODEL_H
