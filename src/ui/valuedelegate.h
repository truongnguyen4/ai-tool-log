#ifndef VALUEDELEGATE_H
#define VALUEDELEGATE_H

#include <QStyledItemDelegate>
#include <QLineEdit>

class ValueDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ValueDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const override;
    
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                     const QModelIndex &index) const override;
    
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const override;
};

#endif // VALUEDELEGATE_H
