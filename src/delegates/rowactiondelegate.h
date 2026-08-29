#ifndef ROWACTIONDELEGATE_H
#define ROWACTIONDELEGATE_H

#include <QIcon>
#include <QPersistentModelIndex>
#include <QStyledItemDelegate>

class QTableView;

/**
 * Draws a clickable icon button into a table column.
 *
 * The configuration tables used to place a real QPushButton in every row via
 * QTableView::setIndexWidget(). That allocates one widget per row (three for
 * the property-definition table), has to be torn down and rebuilt on every
 * filter keystroke, and — because each button's slot captured its row number —
 * silently acted on the wrong row once any row was inserted or removed.
 *
 * Painting the button instead costs nothing per row, needs no rebuild, and
 * reports the row from the index under the cursor, so it cannot go stale.
 *
 * The owning view needs mouse tracking for hover feedback; use
 * installOn() to wire that up.
 */
class RowActionDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    RowActionDelegate(const QIcon &icon, const QString &tooltip,
                      QObject *parent = nullptr);

    /** Attach to @p column of @p view and enable the hover feedback it needs. */
    void installOn(QTableView *view, int column);

    /** Disabled actions paint dimmed and ignore clicks. */
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

signals:
    /** The user activated the action on @p row. */
    void triggered(int row);

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

private:
    /** Square button rect centred inside the cell. */
    static QRect buttonRect(const QStyleOptionViewItem &option);
    void repaintCell(const QStyleOptionViewItem &option) const;

    QIcon   m_icon;
    QString m_tooltip;
    bool    m_enabled = true;

    QPersistentModelIndex m_hovered;
    QPersistentModelIndex m_pressed;
};

#endif // ROWACTIONDELEGATE_H
