#ifndef HIGHLIGHTDELEGATE_H
#define HIGHLIGHTDELEGATE_H

#include <QColor>
#include <QHash>
#include <QStringList>
#include <QStyledItemDelegate>

/**
 * Paints table cells with the active filter keywords highlighted.
 *
 * Used on the log table's PID / Package / Tag / Message columns so the terms
 * the user filtered or searched for stand out. Each keyword gets a distinct
 * background colour, assigned in order.
 *
 * The Message column additionally word-wraps, which means paint() and
 * sizeHint() have to measure text themselves rather than deferring to the
 * base class.
 */
class HighlightDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit HighlightDelegate(QObject *parent = nullptr);

    /** Replace the highlighted keywords; colours are assigned automatically. */
    void setKeywords(const QStringList &keywords);
    void clearKeywords();
    bool hasKeywords() const { return !m_keywords.isEmpty(); }

    /**
     * Enable word wrapping. sizeHint() then reports the wrapped height and
     * paint() lays the text out across multiple lines.
     */
    void setWordWrap(bool enabled);
    bool wordWrap() const { return m_wordWrap; }

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    /** Draw the item's chrome: style background plus any marked-row tint. */
    void drawItemBackground(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const;
    /** Plain text, wrapped or single-line, with no keyword spans. */
    void drawPlainText(QPainter *painter, const QStyleOptionViewItem &option,
                       const QString &text, bool selected) const;
    /** Single-line text with keyword backgrounds, elided at the cell edge. */
    void drawHighlightedText(QPainter *painter, const QStyleOptionViewItem &option,
                             const QString &text, bool selected) const;
    /** Wrapped text with keyword backgrounds, laid out via QTextDocument. */
    void drawWrappedHighlightedText(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QString &text, bool selected) const;

    /** True when at least one keyword occurs in @p text. */
    bool containsAnyKeyword(const QString &text) const;
    /** Usable text width of a cell in @p option, excluding padding. */
    static int contentWidth(const QStyleOptionViewItem &option, const QModelIndex &index);

    QStringList           m_keywords;
    QHash<QString, QColor> m_colors;
    bool                  m_wordWrap = false;
};

#endif // HIGHLIGHTDELEGATE_H
