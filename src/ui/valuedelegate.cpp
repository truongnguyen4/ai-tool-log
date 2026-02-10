#include "valuedelegate.h"

ValueDelegate::ValueDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *ValueDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    
    QLineEdit *editor = new QLineEdit(parent);
    editor->setStyleSheet("QLineEdit { "
                         "background-color: #2d2d30; "
                         "border: 1px solid #3e3e42; "
                         "color: #cccccc; "
                         "padding: 1px; "
                         "margin: 1px; "
                         "}");
    return editor;
}

void ValueDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
    lineEdit->setText(value);
}

void ValueDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                const QModelIndex &index) const
{
    QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
    QString value = lineEdit->text();
    model->setData(index, value, Qt::EditRole);
}

void ValueDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    Q_UNUSED(index)
    editor->setGeometry(option.rect);
}
