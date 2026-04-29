#ifndef SETTINGSMODEL_H
#define SETTINGSMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QHash>
#include <QString>
#include <QElapsedTimer>
#include "valuefilter.h"
#include "settingentry.h"

class QTimer;

class SettingsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit SettingsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    
    void setSettings(const QVector<SettingEntry> &settings);
    // Update values in the model.
    // allowInsert=true (default): add entries not yet present (adb fetch path).
    // allowInsert=false: update value-only for existing entries, no insertion (socket path).
    void updateSettings(const QVector<SettingEntry> &settings, bool allowInsert = true);
    const QVector<SettingEntry>& getSettings() const;
    // Returns the rows currently visible in the table (filtered if a filter
    // is active, otherwise all rows).
    const QVector<SettingEntry>& visibleSettings() const;
    
    void applyFilter(const QString &nameFilter, const QString &valueFilter = QString());
    void reapplyFilter();
    void clearFilter();

private:
    QVector<SettingEntry> m_allSettings;
    QVector<SettingEntry> m_filteredSettings;
    ValueFilter m_filter;
    bool m_isFiltered;
    QString m_currentNameFilter;   // Store current name filter to reapply after update
    QString m_currentValueFilter;  // Store current value filter to reapply after update

    // Blink-on-change: per-setting-key millisecond timestamp of the most
    // recent value change. data() with BackgroundRole returns a highlight
    // color for ~1s after the change; a single sweep timer expires entries.
    QHash<QString, qint64> m_blinkUntil;   // setting key -> deadline ms
    QElapsedTimer          m_clock;
    QTimer                *m_blinkSweep = nullptr;
    void scheduleBlinkSweep();
};

#endif // SETTINGSMODEL_H
