#ifndef SEARCHABLELISTPOPUP_H
#define SEARCHABLELISTPOPUP_H

#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

/**
 * A drop-down list to pick one name out of many.
 *
 * A menu is the wrong shape once a list runs to hundreds of entries — a
 * device runs that many processes — so this is a scrollable list with a
 * search box above it: type to narrow, arrows to walk, Enter to pick.
 *
 * Rows may be headers, which only label the rows under them and disappear
 * while a search is running.
 */
class SearchableListPopup : public QWidget
{
    Q_OBJECT

public:
    /** One row: a name to pick, or a header labelling the rows under it. */
    struct Item {
        QString text;
        bool    header = false;
    };

    explicit SearchableListPopup(QWidget *parent = nullptr);

    void setPlaceholderText(const QString &text);
    /** Noun for the row count under the list ("apps", "services", …). */
    void setItemLabel(const QString &plural);

    /** Show @p items under @p anchor with the search box ready for typing. */
    void showItems(QWidget *anchor, const QVector<Item> &items);

signals:
    void itemChosen(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /** Hide the rows that do not contain @p needle, headers included. */
    void applyFilter(const QString &needle);
    /** Move the current row @p delta steps, skipping hidden rows and headers. */
    void step(int delta);
    void choose(QListWidgetItem *item);

    QLineEdit   *m_search = nullptr;
    QListWidget *m_list   = nullptr;
    QLabel      *m_count  = nullptr;
    QString      m_itemLabel;
};

#endif // SEARCHABLELISTPOPUP_H
