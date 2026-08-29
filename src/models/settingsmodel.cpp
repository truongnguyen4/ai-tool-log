#include "settingsmodel.h"
#include "blinksweep.h"
#include "colorscheme.h"
#include "tableconfig.h"
#include "valuetablesync.h"
#include <QBrush>
#include <QSet>
#include <QTimer>

namespace {
/**
 * Identity aliases of a setting, most specific first.
 *
 * The namespaced "<group>\x1f<setting>" key is the primary identity; the bare
 * setting name is an alias so that a push carrying no namespace still lands on
 * the right row.
 */
QStringList settingAliases(const SettingEntry &entry)
{
    if (entry.group.isEmpty())
        return {entry.setting};
    return {entry.group + QLatin1Char('\x1f') + entry.setting, entry.setting};
}

/** Primary key of a setting — matches settingAliases().first(). */
QString settingKey(const SettingEntry &entry)
{
    return settingAliases(entry).value(0);
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

    // Blink-on-change: tint the row for ~1s after updateSettings() saw the
    // value move, so the user can spot what changed during live monitoring.
    if (role == Qt::BackgroundRole && !m_blinkUntil.isEmpty()) {
        const auto it = m_blinkUntil.constFind(settingKey(entry));
        if (it != m_blinkUntil.constEnd() && it.value() > m_clock.elapsed())
            return QBrush(ColorScheme::instance().blinkBackground());
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
    if (!index.isValid() || role != Qt::EditRole
        || index.column() != TableConfig::SettingsColumns::VALUE)
        return false;

    QVector<SettingEntry> &settings = m_isFiltered ? m_filteredSettings : m_allSettings;
    if (index.row() < 0 || index.row() >= settings.size())
        return false;

    const QString newValue = value.toString();
    settings[index.row()].value = newValue;

    // The filtered list holds copies, so mirror the edit into the master list.
    if (m_isFiltered) {
        const SettingEntry &edited = settings.at(index.row());
        for (SettingEntry &candidate : m_allSettings) {
            if (candidate.group == edited.group && candidate.setting == edited.setting) {
                candidate.value = newValue;
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
    const auto merged = ValueTableSync::merge(m_allSettings, settings,
                                              allowInsert, settingAliases);
    if (!merged.changed)
        return;

    // Refresh blink deadlines for any rows whose value changed.
    if (!merged.changedKeys.isEmpty()) {
        const qint64 deadline = m_clock.elapsed() + BlinkSweep::kBlinkDurationMs;
        for (const QString &key : merged.changedKeys)
            m_blinkUntil.insert(key, deadline);   // overwrites earlier deadline
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
        ValueTableSync::copyValues(m_filteredSettings, m_allSettings, settingAliases);

    const int rows = rowCount();
    if (rows > 0)
        emit dataChanged(index(0, 0), index(rows - 1, columnCount() - 1));
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
