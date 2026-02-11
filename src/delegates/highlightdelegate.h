#ifndef HIGHLIGHTDELEGATE_H
#define HIGHLIGHTDELEGATE_H

#include <QStyledItemDelegate>
#include <QStringList>
#include <QColor>
#include <QMap>

/**
 * @brief Delegate that highlights keywords in table cells with different colors
 * 
 * This delegate is used to highlight filter keywords in the Tag and Message columns
 * of the log table. Each keyword gets a distinct color for easy identification.
 */
class HighlightDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit HighlightDelegate(QObject *parent = nullptr);
    
    /**
     * @brief Set the keywords to highlight with automatic color assignment
     * @param keywords List of keywords to highlight
     */
    void setKeywords(const QStringList &keywords);
    
    /**
     * @brief Clear all keywords (disables highlighting)
     */
    void clearKeywords();
    
    /**
     * @brief Check if highlighting is enabled (has keywords)
     */
    bool hasKeywords() const;

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    /**
     * @brief Draw text with highlighted keywords
     */
    void drawHighlightedText(QPainter *painter, const QStyleOptionViewItem &option,
                            const QString &text, bool isSelected) const;
    
    /**
     * @brief Generate a distinct color for a keyword based on its index
     */
    QColor getColorForKeyword(int index) const;
    
    QStringList m_keywords;           // Keywords to highlight
    QMap<QString, QColor> m_colors;   // Color mapping for each keyword
    
    // Predefined highlight colors (background colors for keywords)
    static const QColor HIGHLIGHT_COLORS[];
    static const int HIGHLIGHT_COLOR_COUNT;
};

#endif // HIGHLIGHTDELEGATE_H
