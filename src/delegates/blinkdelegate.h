#ifndef BLINKDELEGATE_H
#define BLINKDELEGATE_H

#include <QStyledItemDelegate>

// Default item delegate that honours the model's Qt::BackgroundRole brush even
// when QSS rules on QTableView::item would otherwise suppress the default
// background fill. Used so the row-wide blink-on-change highlight is visible
// across every column.
class BlinkDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit BlinkDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

#endif // BLINKDELEGATE_H
