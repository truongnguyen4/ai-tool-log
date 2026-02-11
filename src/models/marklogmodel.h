#ifndef MARKLOGMODEL_H
#define MARKLOGMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QColor>
#include "ilogconverter.h"

struct MarkedLogEntry {
    LogEntry entry;
    int originalIndex; // Index in the main log table
};

class MarkLogModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit MarkLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Custom methods
    void addMarkedLog(const LogEntry &entry, int originalIndex);
    void removeMarkedLog(int originalIndex);
    bool isMarked(int originalIndex) const;
    int getOriginalIndex(int row) const;
    void clear();
    int getMarkedCount() const;

private:
    QVector<MarkedLogEntry> m_markedLogs;
    QColor getLevelColor(const QString &level) const;
};

#endif // MARKLOGMODEL_H
