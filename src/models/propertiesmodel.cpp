#include "propertiesmodel.h"
#include "tableconfig.h"

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
    return TableConfig::PropertiesColumns::TOTAL_COLUMNS;
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
        using namespace TableConfig::PropertiesColumns;
        switch (index.column()) {
        case LINE: return entry.line;
        case PROPERTY: return entry.property;
        case VALUE: return entry.value;
        case ACTION: return QString(); // Action column (for button)
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
        using namespace TableConfig::PropertiesColumns;
        switch (section) {
        case LINE: return "LINE";
        case PROPERTY: return "PROPERTY";
        case VALUE: return "VALUE";
        case ACTION: return "";
        default: return QVariant();
        }
    }

    return QVariant();
}

Qt::ItemFlags PropertiesModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    // Make VALUE column editable
    if (index.column() == TableConfig::PropertiesColumns::VALUE)
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
        applyFilter(m_currentNameFilter, m_currentValueFilter);
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

void PropertiesModel::applyFilter(const QString &nameFilter, const QString &valueFilter)
{
    beginResetModel();
    
    m_currentNameFilter = nameFilter;
    m_currentValueFilter = valueFilter;
    
    if (nameFilter.isEmpty() && valueFilter.isEmpty()) {
        m_isFiltered = false;
        m_filteredProperties.clear();
    } else {
        m_isFiltered = true;
        m_filteredProperties.clear();
        
        ConfigFilterCriteria criteria;
        criteria.nameFilter = nameFilter;
        criteria.valueFilter = valueFilter;
        
        for (const PropertyEntry &entry : m_allProperties) {
            // Filter by PROPERTY name and by VALUE
            if (m_filter.passesFilter(entry.property, entry.value, criteria)) {
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
