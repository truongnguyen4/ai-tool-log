#include "propertiesmodel.h"
#include "blinksweep.h"
#include "colorscheme.h"
#include "tableconfig.h"
#include "valuetablesync.h"
#include <QBrush>
#include <QSet>
#include <QTimer>

namespace {
/** Identity of a system property — its name is already unique. */
QStringList propertyAliases(const PropertyEntry &entry)
{
    return {entry.property};
}
}

PropertiesModel::PropertiesModel(QObject *parent)
    : QAbstractTableModel(parent), m_isFiltered(false)
{
    m_clock.start();
    m_blinkSweep = new QTimer(this);
    BlinkSweep::installForModel(m_blinkSweep, &m_blinkUntil, &m_clock, this);
}

void PropertiesModel::scheduleBlinkSweep()
{
    if (!m_blinkSweep->isActive()) m_blinkSweep->start();
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

    // Blink-on-change: tint the row for ~1s after updateProperties() saw the
    // value move, so the user can spot what changed during live monitoring.
    if (role == Qt::BackgroundRole && !m_blinkUntil.isEmpty()) {
        const auto it = m_blinkUntil.constFind(entry.property);
        if (it != m_blinkUntil.constEnd() && it.value() > m_clock.elapsed())
            return QBrush(ColorScheme::instance().blinkBackground());
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
    if (!index.isValid() || role != Qt::EditRole
        || index.column() != TableConfig::PropertiesColumns::VALUE)
        return false;

    QVector<PropertyEntry> &properties = m_isFiltered ? m_filteredProperties : m_allProperties;
    if (index.row() < 0 || index.row() >= properties.size())
        return false;

    const QString newValue = value.toString();
    properties[index.row()].value = newValue;

    // The filtered list holds copies, so mirror the edit into the master list.
    // Match on the property name: unlike the line number it is unique.
    if (m_isFiltered) {
        const QString &name = properties.at(index.row()).property;
        for (PropertyEntry &candidate : m_allProperties) {
            if (candidate.property == name) {
                candidate.value = newValue;
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
    const auto merged = ValueTableSync::merge(m_allProperties, properties,
                                              allowInsert, propertyAliases);
    if (!merged.changed)
        return;

    if (!merged.changedKeys.isEmpty()) {
        const qint64 deadline = m_clock.elapsed() + BlinkSweep::kBlinkDurationMs;
        for (const QString &key : merged.changedKeys)
            m_blinkUntil.insert(key, deadline);
        scheduleBlinkSweep();
    }

    if (allowInsert) {
        // Full upsert path: rebuild filter and reset view.
        if (m_isFiltered) {
            applyFilter(m_currentNameFilter, m_currentValueFilter);
        } else {
            beginResetModel();
            endResetModel();
        }
        return;
    }

    // Value-only monitor path: refresh the filtered view in place and repaint
    // without resetting, so selection and scroll position survive each tick.
    if (m_isFiltered)
        ValueTableSync::copyValues(m_filteredProperties, m_allProperties, propertyAliases);

    const int rows = rowCount();
    if (rows > 0)
        emit dataChanged(index(0, 0), index(rows - 1, columnCount() - 1));
}

const QVector<PropertyEntry>& PropertiesModel::getProperties() const
{
    return m_allProperties;
}

const QVector<PropertyEntry>& PropertiesModel::visibleProperties() const
{
    return m_isFiltered ? m_filteredProperties : m_allProperties;
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
        
        ValueFilterCriteria criteria;
        criteria.nameFilter  = nameFilter;
        criteria.parsedName  = ParsedFilter::build(nameFilter);
        criteria.valueFilter = valueFilter;
        criteria.parsedValue = ParsedFilter::build(valueFilter);

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
