#include "settingsmodel.h"
#include "tableconfig.h"
#include <QDebug>

SettingsModel::SettingsModel(QObject *parent)
    : QAbstractTableModel(parent), m_isFiltered(false)
{
}

int SettingsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_isFiltered ? m_filteredSettings.size() : m_allSettings.size();
}

int SettingsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return TableConfig::SettingsColumns::TOTAL_COLUMNS;
}

QVariant SettingsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    
    const QVector<SettingEntry> &settings = m_isFiltered ? m_filteredSettings : m_allSettings;
    
    if (index.row() >= settings.size())
        return QVariant();

    const SettingEntry &entry = settings[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        using namespace TableConfig::SettingsColumns;
        switch (index.column()) {
        case LINE: return entry.line;
        case GROUP: return entry.group;
        case SETTING: return entry.setting;
        case VALUE: return entry.value;
        case ACTION: return QString(); // Action column (for button)
        default: return QVariant();
        }
    }

    return QVariant();
}

QVariant SettingsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        using namespace TableConfig::SettingsColumns;
        switch (section) {
        case LINE:    return Names::LINE;
        case GROUP:   return Names::GROUP;
        case SETTING: return Names::SETTING;
        case VALUE:   return Names::VALUE;
        case ACTION:  return Names::ACTION;
        default: return QVariant();
        }
    }

    return QVariant();
}

Qt::ItemFlags SettingsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    // Make VALUE column editable
    if (index.column() == TableConfig::SettingsColumns::VALUE)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool SettingsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || index.column() != 3)
        return false;
    
    QVector<SettingEntry> &settings = m_isFiltered ? m_filteredSettings : m_allSettings;
    
    if (index.row() >= settings.size())
        return false;
    
    settings[index.row()].value = value.toString();
    
    // If filtering, also update in all settings
    if (m_isFiltered) {
        const QString &line = settings[index.row()].line;
        for (int i = 0; i < m_allSettings.size(); ++i) {
            if (m_allSettings[i].line == line) {
                m_allSettings[i].value = value.toString();
                break;
            }
        }
    }
    
    emit dataChanged(index, index, {role});
    return true;
}

void SettingsModel::setSettings(const QVector<SettingEntry> &settings)
{
    beginResetModel();
    m_allSettings = settings;
    m_filteredSettings.clear();
    // Do NOT reset m_isFiltered — preserve filter state so that reapplyFilter()
    // applies the existing filter when new data arrives after a device reconnect.
    endResetModel();
}

void SettingsModel::updateSettings(const QVector<SettingEntry> &settings, bool allowInsert)
{
    bool changed = false;

    for (const SettingEntry &newEntry : settings) {
        bool found = false;
        for (int i = 0; i < m_allSettings.size(); ++i) {
            const bool match = allowInsert
                ? (m_allSettings[i].group == newEntry.group && m_allSettings[i].setting == newEntry.setting)
                : (m_allSettings[i].setting == newEntry.setting);

            if (match) {
                m_allSettings[i].value = newEntry.value;
                if (allowInsert)
                    m_allSettings[i].line = newEntry.line;
                found   = true;
                changed = true;
                break;
            }
        }
        if (!found && allowInsert) {
            m_allSettings.append(newEntry);
            changed = true;
        }
    }

    if (!changed)
        return;

    if (allowInsert) {
        // Full upsert path: rebuild filter and reset view.
        if (m_isFiltered)
            applyFilter(m_currentNameFilter, m_currentValueFilter);
        else {
            beginResetModel();
            endResetModel();
        }
    } else {
        // Value-only socket path: sync filtered list and emit targeted dataChanged.
        if (m_isFiltered) {
            for (SettingEntry &fe : m_filteredSettings) {
                for (const SettingEntry &e : m_allSettings) {
                    if (e.setting == fe.setting) {
                        fe.value = e.value;
                        break;
                    }
                }
            }
        }
        const int rows = rowCount();
        if (rows > 0) {
            emit dataChanged(index(0, TableConfig::SettingsColumns::VALUE),
                             index(rows - 1, TableConfig::SettingsColumns::VALUE));
        }
    }
}

const QVector<SettingEntry>& SettingsModel::getSettings() const
{
    return m_allSettings;
}

void SettingsModel::applyFilter(const QString &nameFilter, const QString &valueFilter)
{
    beginResetModel();
    
    m_currentNameFilter = nameFilter;
    m_currentValueFilter = valueFilter;
    
    if (nameFilter.isEmpty() && valueFilter.isEmpty()) {
        m_isFiltered = false;
        m_filteredSettings.clear();
    } else {
        m_isFiltered = true;
        m_filteredSettings.clear();
        
        ConfigFilterCriteria criteria;
        criteria.nameFilter = nameFilter;
        criteria.valueFilter = valueFilter;
        
        for (const SettingEntry &entry : m_allSettings) {
            // Filter by GROUP or SETTING name, and by VALUE
            QString combinedName = entry.group + " " + entry.setting;
            if (m_filter.passesFilter(combinedName, entry.value, criteria)) {
                m_filteredSettings.append(entry);
            }
        }
    }
    
    endResetModel();
}

void SettingsModel::reapplyFilter()
{
    if (m_isFiltered)
        applyFilter(m_currentNameFilter, m_currentValueFilter);
}

void SettingsModel::clearFilter()
{
    applyFilter(QString());
}
