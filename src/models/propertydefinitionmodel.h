#ifndef PROPERTYDEFINITIONMODEL_H
#define PROPERTYDEFINITIONMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include "propertydefinition.h"

class PropertyDefinitionModel : public QAbstractTableModel
{
    Q_OBJECT
    
public:
    explicit PropertyDefinitionModel(QObject *parent = nullptr);
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void setPropertyDefinitions(const QVector<PropertyDefinition> &properties);
    void addPropertyDefinition(const PropertyDefinition &property);
    void removePropertyDefinition(int row);
    void clear();
    
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    
    const QVector<PropertyDefinition>& getPropertyDefinitions() const { return m_properties; }
    
private:
    QVector<PropertyDefinition> m_properties;
};

#endif // PROPERTYDEFINITIONMODEL_H
