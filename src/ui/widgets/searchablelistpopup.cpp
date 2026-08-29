#include "searchablelistpopup.h"

#include <QAbstractItemView>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>

namespace {
/** Rows a header owns are only labelled by it; it is never a choice. */
constexpr int kHeaderRole = Qt::UserRole + 1;
constexpr int kMinWidth   = 280;
constexpr int kHeight     = 340;
/** Gap kept between the popup and the edge of the screen. */
constexpr int kScreenMargin = 8;
} // namespace

SearchableListPopup::SearchableListPopup(QWidget *parent)
    : QWidget(parent, Qt::Popup)
    , m_itemLabel(tr("items"))
{
    setObjectName(QStringLiteral("searchableListPopup"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 6);
    layout->setSpacing(6);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("searchableListSearch"));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("searchableListView"));
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setFocusPolicy(Qt::NoFocus);   // the search box keeps the keyboard
    layout->addWidget(m_list, 1);

    m_count = new QLabel(this);
    m_count->setObjectName(QStringLiteral("searchableListCount"));
    layout->addWidget(m_count);

    connect(m_search, &QLineEdit::textChanged, this, &SearchableListPopup::applyFilter);
    connect(m_search, &QLineEdit::returnPressed, this,
            [this]() { choose(m_list->currentItem()); });
    connect(m_list, &QListWidget::itemClicked, this, &SearchableListPopup::choose);
    m_search->installEventFilter(this);
}

void SearchableListPopup::setPlaceholderText(const QString &text)
{
    m_search->setPlaceholderText(text);
}

void SearchableListPopup::setItemLabel(const QString &plural)
{
    m_itemLabel = plural;
}

void SearchableListPopup::showItems(QWidget *anchor, const QVector<Item> &items)
{
    m_list->clear();
    for (const Item &item : items) {
        auto *row = new QListWidgetItem(item.text, m_list);
        if (item.header) {
            row->setFlags(Qt::NoItemFlags);
            row->setData(kHeaderRole, true);
        }
    }
    m_search->clear();
    applyFilter(QString());

    resize(qMax(anchor ? anchor->width() : 0, kMinWidth), kHeight);
    if (anchor) {
        // Under the button, unless the screen ends first.
        QPoint topLeft = anchor->mapToGlobal(QPoint(0, anchor->height()));
        const QScreen *screen = anchor->screen() ? anchor->screen() : QGuiApplication::primaryScreen();
        if (screen) {
            const QRect available = screen->availableGeometry();
            if (topLeft.y() + height() > available.bottom() - kScreenMargin)
                topLeft.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height());
            topLeft.setX(qBound(available.left() + kScreenMargin, topLeft.x(),
                                available.right() - width() - kScreenMargin));
        }
        move(topLeft);
    }

    show();
    m_search->setFocus();
}

bool SearchableListPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_search || event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);

    // The list is walked from the search box, so typing never stops.
    switch (static_cast<QKeyEvent *>(event)->key()) {
    case Qt::Key_Down:     step(1);                       return true;
    case Qt::Key_Up:       step(-1);                      return true;
    case Qt::Key_PageDown: step(m_list->count() / 10 + 1); return true;
    case Qt::Key_PageUp:   step(-(m_list->count() / 10 + 1)); return true;
    case Qt::Key_Escape:   close();                       return true;
    default:               break;
    }
    return QWidget::eventFilter(watched, event);
}

void SearchableListPopup::applyFilter(const QString &needle)
{
    const QString trimmed = needle.trimmed();
    const bool searching = !trimmed.isEmpty();

    int shown = 0;
    int total = 0;
    QListWidgetItem *firstMatch = nullptr;
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem *item = m_list->item(row);
        if (item->data(kHeaderRole).toBool()) {
            item->setHidden(searching);   // a section title means nothing mid-search
            continue;
        }
        ++total;
        const bool matches = !searching || item->text().contains(trimmed, Qt::CaseInsensitive);
        item->setHidden(!matches);
        if (!matches)
            continue;
        ++shown;
        if (!firstMatch)
            firstMatch = item;
    }

    if (firstMatch) {
        m_list->setCurrentItem(firstMatch);
        // While searching the first hit leads; with no search the list starts
        // at its own top, header included.
        if (searching)
            m_list->scrollToItem(firstMatch, QAbstractItemView::PositionAtTop);
        else
            m_list->scrollToTop();
    }
    m_count->setText(searching ? tr("%1 of %2 %3").arg(shown).arg(total).arg(m_itemLabel)
                               : tr("%1 %2").arg(total).arg(m_itemLabel));
}

void SearchableListPopup::step(int delta)
{
    const int count = m_list->count();
    if (count == 0 || delta == 0)
        return;

    const int direction = delta > 0 ? 1 : -1;
    int remaining = qAbs(delta);
    int row = m_list->currentRow();
    for (int tried = 0; tried < count && remaining > 0; ++tried) {
        row += direction;
        if (row < 0)
            row = count - 1;
        else if (row >= count)
            row = 0;
        const QListWidgetItem *item = m_list->item(row);
        if (item->isHidden() || !item->flags().testFlag(Qt::ItemIsSelectable))
            continue;
        m_list->setCurrentRow(row);
        --remaining;
    }
    if (QListWidgetItem *current = m_list->currentItem())
        m_list->scrollToItem(current);
}

void SearchableListPopup::choose(QListWidgetItem *item)
{
    if (!item || item->isHidden() || !item->flags().testFlag(Qt::ItemIsSelectable))
        return;
    const QString text = item->text();
    close();
    emit itemChosen(text);
}
