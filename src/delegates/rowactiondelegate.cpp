#include "rowactiondelegate.h"

#include "colorscheme.h"

#include <QHeaderView>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTableView>
#include <QToolTip>

namespace {
constexpr int kButtonSize  = 22;   ///< square button edge, in pixels
constexpr int kIconSize    = 14;
constexpr int kCornerRadius = 4;
constexpr qreal kDisabledOpacity = 0.35;
} // namespace

RowActionDelegate::RowActionDelegate(const QIcon &icon, const QString &tooltip,
                                     QObject *parent)
    : QStyledItemDelegate(parent)
    , m_icon(icon)
    , m_tooltip(tooltip)
{
}

void RowActionDelegate::installOn(QTableView *view, int column)
{
    if (!view)
        return;
    view->setItemDelegateForColumn(column, this);
    // editorEvent() only sees MouseMove — and can therefore only track hover —
    // once the *viewport* reports moves with no button held down.
    view->setMouseTracking(true);
    view->viewport()->setMouseTracking(true);
}

void RowActionDelegate::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    m_hovered = QPersistentModelIndex();
    m_pressed = QPersistentModelIndex();
}

QRect RowActionDelegate::buttonRect(const QStyleOptionViewItem &option)
{
    QRect r(0, 0, kButtonSize, kButtonSize);
    r.moveCenter(option.rect.center());
    return r;
}

void RowActionDelegate::repaintCell(const QStyleOptionViewItem &option) const
{
    if (auto *view = qobject_cast<QAbstractItemView *>(
            const_cast<QWidget *>(option.widget)))
        view->viewport()->update();
}

void RowActionDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    painter->save();

    const QRect rect = buttonRect(option);
    const bool hovered = m_enabled && m_hovered.isValid() && m_hovered == index;
    const bool pressed = m_enabled && m_pressed.isValid() && m_pressed == index;

    if (hovered || pressed) {
        QColor fill = ColorScheme::instance().accent();
        fill.setAlpha(pressed ? 90 : 50);
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(rect, kCornerRadius, kCornerRadius);
    }

    if (!m_enabled)
        painter->setOpacity(kDisabledOpacity);

    QRect iconRect(0, 0, kIconSize, kIconSize);
    iconRect.moveCenter(rect.center());
    m_icon.paint(painter, iconRect, Qt::AlignCenter,
                 m_enabled ? QIcon::Normal : QIcon::Disabled);

    painter->restore();
}

QSize RowActionDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(kButtonSize + 8, kButtonSize + 4);
}

bool RowActionDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                    const QStyleOptionViewItem &option,
                                    const QModelIndex &index)
{
    Q_UNUSED(model);
    if (!m_enabled || !index.isValid())
        return false;

    switch (event->type()) {
    case QEvent::MouseMove: {
        const auto *me = static_cast<QMouseEvent *>(event);
        const QPersistentModelIndex next =
            buttonRect(option).contains(me->pos()) ? QPersistentModelIndex(index)
                                                   : QPersistentModelIndex();
        if (next != m_hovered) {
            m_hovered = next;
            repaintCell(option);
        }
        return false;
    }

    case QEvent::MouseButtonPress: {
        const auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton || !buttonRect(option).contains(me->pos()))
            return false;
        m_pressed = index;
        repaintCell(option);
        return true;
    }

    case QEvent::MouseButtonRelease: {
        const auto *me = static_cast<QMouseEvent *>(event);
        const bool wasPressedHere = m_pressed.isValid() && m_pressed == index;
        m_pressed = QPersistentModelIndex();
        repaintCell(option);
        if (me->button() != Qt::LeftButton || !wasPressedHere
            || !buttonRect(option).contains(me->pos()))
            return false;
        // Read the row from the live index, never from a captured copy.
        emit triggered(index.row());
        return true;
    }

    default:
        return false;
    }
}

bool RowActionDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index)
{
    if (event && event->type() == QEvent::ToolTip && !m_tooltip.isEmpty()) {
        QToolTip::showText(event->globalPos(), m_tooltip, view);
        return true;
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}
