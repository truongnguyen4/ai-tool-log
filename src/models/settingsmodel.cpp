#include "settingsmodel.h"

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
    return 5; // LINE, GROUP, SETTING, VALUE, ACTION
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
        switch (index.column()) {
        case 0: return entry.line;
        case 1: return entry.group;
        case 2: return entry.setting;
        case 3: return entry.value;
        case 4: return QString(); // Action column (for button)
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
        switch (section) {
        case 0: return "LINE";
        case 1: return "GROUP";
        case 2: return "SETTING";
        case 3: return "VALUE";
        case 4: return "";
        default: return QVariant();
        }
    }

    return QVariant();
}

Qt::ItemFlags SettingsModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    
    // Make VALUE column (column 3) editable
    if (index.column() == 3)
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
    m_isFiltered = false;
    endResetModel();
}

void SettingsModel::updateSettings(const QVector<SettingEntry> &settings)
{
    // Store current filter state
    bool wasFiltered = m_isFiltered;
    QString filterText = m_currentFilterText;
    
    // Update or add settings in m_allSettings
    for (const SettingEntry &newEntry : settings) {
        bool found = false;
        
        // Find existing entry by group and setting name
        for (int i = 0; i < m_allSettings.size(); ++i) {
            if (m_allSettings[i].group == newEntry.group && 
                m_allSettings[i].setting == newEntry.setting) {
                // Update existing entry
                m_allSettings[i].value = newEntry.value;
                m_allSettings[i].line = newEntry.line;
                found = true;
                break;
            }
        }
        
        // If not found, add new entry
        if (!found) {
            m_allSettings.append(newEntry);
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

const QVector<SettingEntry>& SettingsModel::getSettings() const
{
    return m_allSettings;
}

void SettingsModel::applyFilter(const QString &filterText)
{
    beginResetModel();
    
    m_currentFilterText = filterText;  // Store filter text
    
    if (filterText.isEmpty()) {
        m_isFiltered = false;
        m_filteredSettings.clear();
    } else {
        m_isFiltered = true;
        m_filteredSettings.clear();
        
        ConfigFilterCriteria criteria;
        criteria.nameFilter = filterText;
        
        for (const SettingEntry &entry : m_allSettings) {
            // Filter by GROUP or SETTING name
            QString combinedName = entry.group + " " + entry.setting;
            if (m_filter.passesFilter(combinedName, criteria)) {
                m_filteredSettings.append(entry);
            }
        }
    }
    
    endResetModel();
}

void SettingsModel::clearFilter()
{
    applyFilter(QString());
}
