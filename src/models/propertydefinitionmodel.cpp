#include "propertydefinitionmodel.h"
#include "tableconfig.h"

PropertyDefinitionModel::PropertyDefinitionModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int PropertyDefinitionModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_properties.size();
}

int PropertyDefinitionModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return TableConfig::PropertyDefColumns::TOTAL_COLUMNS;
}

QVariant PropertyDefinitionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_properties.size())
        return QVariant();
    
    const PropertyDefinition &prop = m_properties[index.row()];
    
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        using namespace TableConfig::PropertyDefColumns;
        switch (index.column()) {
            case NAME: return prop.name;
            case ID: return prop.id;
            case SUPPORTED: return prop.isSupported ? "Yes" : "No";
            case VALUE: return prop.value;
            case DEFAULT: return prop.defaultValue;
            case NEED_REBOOT: return prop.needReboot ? "Yes" : "No";
            case TYPE: return prop.type;
            case READ_ONLY: return prop.readOnly ? "Yes" : "No";
            case SET_BUTTON: return "";
            case GET_BUTTON: return "";
            case REMOVE_BUTTON: return "";
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        if (index.column() == TableConfig::PropertyDefColumns::SUPPORTED || 
            index.column() == TableConfig::PropertyDefColumns::NEED_REBOOT ||
            index.column() == TableConfig::PropertyDefColumns::READ_ONLY) {
            return Qt::AlignCenter;
        }
    }
    
    return QVariant();
}

QVariant PropertyDefinitionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();
    
    if (orientation == Qt::Horizontal) {
        using namespace TableConfig::PropertyDefColumns;
        switch (section) {
            case NAME: return "Name";
            case ID: return "ID";
            case SUPPORTED: return "Supported";
            case VALUE: return "Value";
            case DEFAULT: return "Default";
            case NEED_REBOOT: return "Need Reboot";
            case TYPE: return "Type";
            case READ_ONLY: return "Read Only";
            case SET_BUTTON: return "";
            case GET_BUTTON: return "";
            case REMOVE_BUTTON: return "";
        }
    }
    
    return QVariant();
}

void PropertyDefinitionModel::setPropertyDefinitions(const QVector<PropertyDefinition> &properties)
{
    beginResetModel();
    m_properties = properties;
    endResetModel();
}

void PropertyDefinitionModel::addPropertyDefinition(const PropertyDefinition &property)
{
    // Check if property already exists
    for (const PropertyDefinition &existing : m_properties) {
        if (existing.name == property.name) {
            // Already exists, don't add duplicate
            return;
        }
    }
    
    beginInsertRows(QModelIndex(), m_properties.size(), m_properties.size());
    m_properties.append(property);
    endInsertRows();
}

void PropertyDefinitionModel::updatePropertyDefinition(int row, const PropertyDefinition &property)
{
    if (row < 0 || row >= m_properties.size())
        return;
    
    // Update the property definition at the specified row
    m_properties[row] = property;
    
    // Emit dataChanged for the entire row
    QModelIndex topLeft = index(row, 0);
    QModelIndex bottomRight = index(row, TableConfig::PropertyDefColumns::TOTAL_COLUMNS - 1);
    emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole, Qt::EditRole});
}

void PropertyDefinitionModel::removePropertyDefinition(int row)
{
    if (row < 0 || row >= m_properties.size())
        return;
    
    beginRemoveRows(QModelIndex(), row, row);
    m_properties.removeAt(row);
    endRemoveRows();
}

void PropertyDefinitionModel::clear()
{
    beginResetModel();
    m_properties.clear();
    endResetModel();
}

Qt::ItemFlags PropertyDefinitionModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    // Only VALUE column is editable
    if (index.column() == TableConfig::PropertyDefColumns::VALUE) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool PropertyDefinitionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_properties.size() || role != Qt::EditRole)
        return false;
    
    // Only VALUE column is editable
    if (index.column() == TableConfig::PropertyDefColumns::VALUE) {
        m_properties[index.row()].value = value.toString();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }
    
    return false;
}
