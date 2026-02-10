#ifndef SETTINGSMODEL_H
#define SETTINGSMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include "iconfigfilter.h"
#include "configfilter.h"

struct SettingEntry {
    QString line;
    QString group;
    QString setting;
    QString value;
};

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
    const QVector<SettingEntry>& getSettings() const;
    
    void applyFilter(const QString &filterText);
    void clearFilter();

private:
    QVector<SettingEntry> m_allSettings;
    QVector<SettingEntry> m_filteredSettings;
    ConfigFilter m_filter;
    bool m_isFiltered;
};

#endif // SETTINGSMODEL_H
