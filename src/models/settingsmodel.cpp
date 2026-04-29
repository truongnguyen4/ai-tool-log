#include "settingsmodel.h"
#include "blinksweep.h"
#include "tableconfig.h"
#include <QBrush>
#include <QColor>
#include <QSet>
#include <QTimer>

namespace {
QString settingKey(const QString &group, const QString &setting)
{
    return group.isEmpty()
               ? setting
               : group + QLatin1Char('\x1f') + setting;
}

QString settingKey(const SettingEntry &entry)
{
    return settingKey(entry.group, entry.setting);
}

bool sameSettingIdentity(const SettingEntry &left, const SettingEntry &right)
{
    if (right.group.isEmpty())
        return left.setting == right.setting;
    return left.group == right.group && left.setting == right.setting;
}
}

SettingsModel::SettingsModel(QObject *parent)
    : QAbstractTableModel(parent), m_isFiltered(false)
{
    m_clock.start();
    m_blinkSweep = new QTimer(this);
    BlinkSweep::installForModel(m_blinkSweep, &m_blinkUntil, &m_clock, this);
}

void SettingsModel::scheduleBlinkSweep()
{
    if (!m_blinkSweep->isActive()) m_blinkSweep->start();
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

    // Blink-on-change: highlight the VALUE cell for ~1s after the value
    // changed via updateSettings(value-only path) so the user can see what
    // moved. Color: warm amber, semi-transparent so it reads on dark theme.
    if (role == Qt::BackgroundRole) {
        const auto it = m_blinkUntil.constFind(settingKey(entry));
        if (it != m_blinkUntil.constEnd() && it.value() > m_clock.elapsed())
            return QBrush(QColor("#1f4d7a"));   // muted blue, fits dark theme
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
        const SettingEntry edited = settings[index.row()];
        for (int i = 0; i < m_allSettings.size(); ++i) {
            if (m_allSettings[i].group == edited.group
                    && m_allSettings[i].setting == edited.setting) {
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
    QSet<QString> blinkKeys;   // settings whose VALUE actually changed

    for (const SettingEntry &newEntry : settings) {
        bool found = false;
        for (int i = 0; i < m_allSettings.size(); ++i) {
            const bool match = sameSettingIdentity(m_allSettings[i], newEntry);

            if (match) {
                if (m_allSettings[i].value != newEntry.value) {
                    blinkKeys.insert(settingKey(m_allSettings[i]));
                    m_allSettings[i].value = newEntry.value;
                    changed = true;
                }
                if (allowInsert)
                    m_allSettings[i].line = newEntry.line;
                found = true;
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

    // Refresh blink deadlines for any rows whose value changed.
    if (!blinkKeys.isEmpty()) {
        const qint64 deadline = m_clock.elapsed() + 1000;
        for (const QString &k : blinkKeys)
            m_blinkUntil.insert(k, deadline);   // overwrites earlier deadline
        scheduleBlinkSweep();
    }

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
                    if (e.group == fe.group && e.setting == fe.setting) {
                        fe.value = e.value;
                        break;
                    }
                }
            }
        }
        const int rows = rowCount();
        if (rows > 0) {
            emit dataChanged(index(0, 0),
                             index(rows - 1, columnCount() - 1));
        }
    }
}

const QVector<SettingEntry>& SettingsModel::getSettings() const
{
    return m_allSettings;
}

const QVector<SettingEntry>& SettingsModel::visibleSettings() const
{
    return m_isFiltered ? m_filteredSettings : m_allSettings;
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
        
        ValueFilterCriteria criteria;
        criteria.nameFilter  = nameFilter;
        criteria.parsedName  = ParsedFilter::build(nameFilter);
        criteria.valueFilter = valueFilter;
        criteria.parsedValue = ParsedFilter::build(valueFilter);

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
