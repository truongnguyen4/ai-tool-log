#include "blinkdelegate.h"
#include <QPainter>
#include <QApplication>
#include <QStyle>
#include <QBrush>

BlinkDelegate::BlinkDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void BlinkDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
{
    const QVariant bg = index.data(Qt::BackgroundRole);
    if (bg.canConvert<QBrush>()) {
        const QBrush b = qvariant_cast<QBrush>(bg);
        if (b.style() != Qt::NoBrush)
            painter->fillRect(option.rect, b);
    }
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.backgroundBrush = QBrush();
    if (const QWidget *w = opt.widget)
        w->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, w);
    else
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter);
}
