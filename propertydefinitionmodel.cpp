#include "propertydefinitionmodel.h"

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
    // Columns: Name, ID, Supported, Loaded, Need Reboot, Volatile, Read Only, Optional, Persistence, eManager, Value, Set, Get, Remove
    return 14;
}

QVariant PropertyDefinitionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_properties.size())
        return QVariant();
    
    const PropertyDefinition &prop = m_properties[index.row()];
    
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
            case 0: return prop.name;
            case 1: return prop.id;
            case 2: return prop.isSupported ? "Yes" : "No";
            case 3: return prop.isLoaded ? "Yes" : "No";
            case 4: return prop.need_reboot ? "Yes" : "No";
            case 5: return prop.volatile_ ? "Yes" : "No";
            case 6: return prop.read_only ? "Yes" : "No";
            case 7: return prop.optional ? "Yes" : "No";
            case 8: return prop.persistence;
            case 9: return prop.eManager;
            case 10: return prop.value; // VALUE column - editable
            case 11: return ""; // Set button column
            case 12: return ""; // Get button column
            case 13: return ""; // Remove button column
        }
    }
    else if (role == Qt::TextAlignmentRole) {
        if (index.column() >= 2 && index.column() <= 7) {
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
        switch (section) {
            case 0: return "Name";
            case 1: return "ID";
            case 2: return "Supported";
            case 3: return "Loaded";
            case 4: return "Need Reboot";
            case 5: return "Volatile";
            case 6: return "Read Only";
            case 7: return "Optional";
            case 8: return "Persistence";
            case 9: return "eManager";
            case 10: return "Value";
            case 11: return "Set";
            case 12: return "Get";
            case 13: return "Remove";
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
    
    // VALUE column (column 10) is editable
    if (index.column() == 10) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool PropertyDefinitionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_properties.size() || role != Qt::EditRole)
        return false;
    
    // Only VALUE column (column 10) is editable
    if (index.column() == 10) {
        m_properties[index.row()].value = value.toString();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }
    
    return false;
}
