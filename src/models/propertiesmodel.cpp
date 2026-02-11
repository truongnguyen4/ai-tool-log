#include "propertiesmodel.h"

PropertiesModel::PropertiesModel(QObject *parent)
    : QAbstractTableModel(parent), m_isFiltered(false)
{
}

int PropertiesModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_isFiltered ? m_filteredProperties.size() : m_allProperties.size();
}

int PropertiesModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 4; // LINE, PROPERTY, VALUE, ACTION
}

QVariant PropertiesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    const QVector<PropertyEntry> &properties = m_isFiltered ? m_filteredProperties : m_allProperties;
    
    if (index.row() >= properties.size())
        return QVariant();

    const PropertyEntry &entry = properties[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case 0: return entry.line;
        case 1: return entry.property;
        case 2: return entry.value;
        case 3: return QString(); // Action column (for button)
        default: return QVariant();
        }
    }

    return QVariant();
}

QVariant PropertiesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return "LINE";
        case 1: return "PROPERTY";
        case 2: return "VALUE";
        case 3: return "";
        default: return QVariant();
        }
    }

    return QVariant();
}

Qt::ItemFlags PropertiesModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    // Make VALUE column (column 2) editable
    if (index.column() == 2)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool PropertiesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || index.column() != 2)
        return false;
    
    QVector<PropertyEntry> &properties = m_isFiltered ? m_filteredProperties : m_allProperties;
    
    if (index.row() >= properties.size())
        return false;
    
    properties[index.row()].value = value.toString();
    
    // If filtering, also update in all properties
    if (m_isFiltered) {
        const QString &line = properties[index.row()].line;
        for (int i = 0; i < m_allProperties.size(); ++i) {
            if (m_allProperties[i].line == line) {
                m_allProperties[i].value = value.toString();
                break;
            }
        }
    }
    
    emit dataChanged(index, index, {role});
    return true;
}

void PropertiesModel::setProperties(const QVector<PropertyEntry> &properties)
{
    beginResetModel();
    m_allProperties = properties;
    m_filteredProperties.clear();
    m_isFiltered = false;
    endResetModel();
}

void PropertiesModel::updateProperties(const QVector<PropertyEntry> &properties)
{
    // Store current filter state
    bool wasFiltered = m_isFiltered;
    QString filterText = m_currentFilterText;
    
    // Update or add properties in m_allProperties
    for (const PropertyEntry &newEntry : properties) {
        bool found = false;
        
        // Find existing entry by property name
        for (int i = 0; i < m_allProperties.size(); ++i) {
            if (m_allProperties[i].property == newEntry.property) {
                // Update existing entry
                m_allProperties[i].value = newEntry.value;
                m_allProperties[i].line = newEntry.line;
                found = true;
                break;
            }
        }
        
        // If not found, add new entry
        if (!found) {
            m_allProperties.append(newEntry);
        }
    }
    
    // Reapply filter if it was active
    if (wasFiltered) {
        applyFilter(filterText);
    } else {
        // No filter, just notify model changed
        beginResetModel();
        endResetModel();
    }
}

const QVector<PropertyEntry>& PropertiesModel::getProperties() const
{
    return m_allProperties;
}

void PropertiesModel::applyFilter(const QString &filterText)
{
    beginResetModel();
    
    m_currentFilterText = filterText;  // Store filter text
    
    if (filterText.isEmpty()) {
        m_isFiltered = false;
        m_filteredProperties.clear();
    } else {
        m_isFiltered = true;
        m_filteredProperties.clear();
        
        ConfigFilterCriteria criteria;
        criteria.nameFilter = filterText;
        
        for (const PropertyEntry &entry : m_allProperties) {
            // Filter by PROPERTY name
            if (m_filter.passesFilter(entry.property, criteria)) {
                m_filteredProperties.append(entry);
            }
        }
    }
    
    endResetModel();
}

void PropertiesModel::clearFilter()
{
    applyFilter(QString());
}
