#ifndef PROPERTYVALUEDELEGATE_H
#define PROPERTYVALUEDELEGATE_H

#include "blinkdelegate.h"

/**
 * Edits a configuration_manager property value with a control that fits its
 * type: a list of the allowed choices for enums, a bounded spin box for
 * ranged integers, a one-character field for chars and a plain field for the
 * rest. Booleans need no editor: the model makes them checkable.
 *
 * Refused values are reported through valueRejected() rather than silently
 * dropped, so the user learns what the property accepts.
 */
class PropertyValueDelegate : public BlinkDelegate
{
    Q_OBJECT

public:
    explicit PropertyValueDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

signals:
    void valueRejected(const QString &reason) const;
};

#endif // PROPERTYVALUEDELEGATE_H
