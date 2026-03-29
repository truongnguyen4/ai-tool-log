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
        case LINE:     return Names::LINE;
        case PROPERTY: return Names::PROPERTY;
        case VALUE:    return Names::VALUE;
        case ACTION:   return Names::ACTION;
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
    // Do NOT reset m_isFiltered — preserve filter state so that reapplyFilter()
    // applies the existing filter when new data arrives after a device reconnect.
    endResetModel();
}

void PropertiesModel::updateProperties(const QVector<PropertyEntry> &properties, bool allowInsert)
{
    bool changed = false;

    for (const PropertyEntry &newEntry : properties) {
        bool found = false;
        for (int i = 0; i < m_allProperties.size(); ++i) {
            if (m_allProperties[i].property == newEntry.property) {
                m_allProperties[i].value = newEntry.value;
                if (allowInsert)
                    m_allProperties[i].line = newEntry.line;
                found   = true;
                changed = true;
                break;
            }
        }
        if (!found && allowInsert) {
            m_allProperties.append(newEntry);
            changed = true;
        }
    }

    if (!changed)
        return;

    if (allowInsert) {
        if (m_isFiltered)
            applyFilter(m_currentNameFilter, m_currentValueFilter);
        else {
            beginResetModel();
            endResetModel();
        }
    } else {
        if (m_isFiltered) {
            for (PropertyEntry &fe : m_filteredProperties) {
                for (const PropertyEntry &e : m_allProperties) {
                    if (e.property == fe.property) {
                        fe.value = e.value;
                        break;
                    }
                }
            }
        }
        const int rows = rowCount();
        if (rows > 0) {
            emit dataChanged(index(0, TableConfig::PropertiesColumns::VALUE),
                             index(rows - 1, TableConfig::PropertiesColumns::VALUE));
        }
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

void PropertiesModel::reapplyFilter()
{
    if (m_isFiltered)
        applyFilter(m_currentNameFilter, m_currentValueFilter);
}

void PropertiesModel::clearFilter()
{
    applyFilter(QString());
}
