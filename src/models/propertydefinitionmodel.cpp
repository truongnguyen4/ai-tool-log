#include "propertydefinitionmodel.h"
#include "blinksweep.h"
#include "tableconfig.h"
#include <QBrush>
#include <QColor>
#include <QSet>
#include <QTimer>

PropertyDefinitionModel::PropertyDefinitionModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_clock.start();
    m_blinkSweep = new QTimer(this);
    BlinkSweep::installForModel(m_blinkSweep, &m_blinkUntil, &m_clock, this);
}

void PropertyDefinitionModel::scheduleBlinkSweep()
{
    if (!m_blinkSweep->isActive()) m_blinkSweep->start();
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
    else if (role == Qt::BackgroundRole) {
        const auto it = m_blinkUntil.constFind(prop.id);
        if (it != m_blinkUntil.constEnd() && it.value() > m_clock.elapsed())
            return QBrush(QColor("#1f4d7a"));
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
            case NAME:          return Names::NAME;
            case ID:            return Names::ID;
            case SUPPORTED:     return Names::SUPPORTED;
            case VALUE:         return Names::VALUE;
            case DEFAULT:       return Names::DEFAULT;
            case NEED_REBOOT:   return Names::NEED_REBOOT;
            case TYPE:          return Names::TYPE;
            case READ_ONLY:     return Names::READ_ONLY;
            case SET_BUTTON:    return "";  // icon-only button column; no header text
            case GET_BUTTON:    return "";  // icon-only button column; no header text
            case REMOVE_BUTTON: return "";  // icon-only button column; no header text
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

void PropertyDefinitionModel::updatePropertyDefinitions(const QVector<PropertyDefinition> &properties, bool allowInsert)
{
    bool changed = false;
    QSet<QString> blinkKeys;

    for (const PropertyDefinition &newEntry : properties) {
        bool found = false;
        for (int i = 0; i < m_properties.size(); ++i) {
            if (m_properties[i].id == newEntry.id) {
                const QString prevValue = m_properties[i].value;
                if (allowInsert) {
                    m_properties[i] = newEntry;
                } else {
                    m_properties[i].value = newEntry.value;
                }
                if (prevValue != newEntry.value)
                    blinkKeys.insert(newEntry.id);
                found   = true;
                changed = true;
                break;
            }
        }
        if (!found && allowInsert) {
            m_properties.append(newEntry);
            changed = true;
        }
    }

    if (!changed)
        return;

    if (!blinkKeys.isEmpty()) {
        const qint64 deadline = m_clock.elapsed() + 1000;
        for (const QString &k : blinkKeys) m_blinkUntil.insert(k, deadline);
        scheduleBlinkSweep();
    }

    if (allowInsert) {
        beginResetModel();
        endResetModel();
    } else {
        m_updatingFromSocket = true;
        emit dataChanged(index(0, 0),
                         index(rowCount() - 1, columnCount() - 1));
        m_updatingFromSocket = false;
    }
}

void PropertyDefinitionModel::updatePropertyDefinition(int row, const PropertyDefinition &property)
{
    if (row < 0 || row >= m_properties.size())
        return;

    const QString prevValue = m_properties[row].value;

    // Update the property definition at the specified row
    m_properties[row] = property;

    // Trigger blink highlight if the value actually changed.
    if (prevValue != property.value && !property.id.isEmpty()) {
        m_blinkUntil.insert(property.id, m_clock.elapsed() + 1000);
        scheduleBlinkSweep();
    }

    // Emit dataChanged for the entire row
    QModelIndex topLeft = index(row, 0);
    QModelIndex bottomRight = index(row, TableConfig::PropertyDefColumns::TOTAL_COLUMNS - 1);
    emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole, Qt::EditRole, Qt::BackgroundRole});
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
