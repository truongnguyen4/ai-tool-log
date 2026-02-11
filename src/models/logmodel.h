#ifndef LOGMODEL_H
#define LOGMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QColor>
#include <QSet>
#include "ilogconverter.h"

class LogModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Custom methods
    void setLogs(const QVector<LogEntry> &logs);
    void addLog(const LogEntry &entry);
    void clear();
    const LogEntry& getLogEntry(int row) const;
    int getLogCount() const;
    void setMarkedRows(const QSet<int> *markedRows);

private:
    QVector<LogEntry> m_logs;
    const QSet<int> *m_markedRows;
    QColor getLevelColor(const QString &level) const;
};

#endif // LOGMODEL_H
