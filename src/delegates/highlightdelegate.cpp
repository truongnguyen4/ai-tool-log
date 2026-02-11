#include "highlightdelegate.h"
#include <QPainter>
#include <QApplication>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>

// Define predefined highlight colors - vibrant but readable
const QColor HighlightDelegate::HIGHLIGHT_COLORS[] = {
    QColor(255, 165, 0, 180),    // Orange
    QColor(135, 206, 250, 180),  // Light Sky Blue
    QColor(144, 238, 144, 180),  // Light Green
    QColor(255, 182, 193, 180),  // Light Pink
    QColor(221, 160, 221, 180),  // Plum
    QColor(255, 255, 0, 180),    // Yellow
    QColor(0, 255, 255, 180),    // Cyan
    QColor(255, 192, 203, 180),  // Pink
    QColor(173, 216, 230, 180),  // Light Blue
    QColor(152, 251, 152, 180),  // Pale Green
};

const int HighlightDelegate::HIGHLIGHT_COLOR_COUNT = 10;

HighlightDelegate::HighlightDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void HighlightDelegate::setKeywords(const QStringList &keywords)
{
    m_keywords = keywords;
    m_colors.clear();
    
    // Assign colors to keywords
    for (int i = 0; i < keywords.size(); i++) {
        m_colors[keywords[i]] = getColorForKeyword(i);
    }
}

void HighlightDelegate::clearKeywords()
{
    m_keywords.clear();
    m_colors.clear();
}

bool HighlightDelegate::hasKeywords() const
{
    return !m_keywords.isEmpty();
}

QColor HighlightDelegate::getColorForKeyword(int index) const
{
    return HIGHLIGHT_COLORS[index % HIGHLIGHT_COLOR_COUNT];
}

void HighlightDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    if (m_keywords.isEmpty()) {
        // No keywords, use default painting
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    
    QString text = index.data(Qt::DisplayRole).toString();
    
    if (text.isEmpty()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    
    // Draw the item background
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    
    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    
    // Clear the text from the option so background is drawn without text
    opt.text = "";
    
    // Draw only the background (including selection), not the text
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    
    // Now draw our highlighted text
    bool isSelected = option.state & QStyle::State_Selected;
    drawHighlightedText(painter, option, text, isSelected);
}

void HighlightDelegate::drawHighlightedText(QPainter *painter, const QStyleOptionViewItem &option,
                                           const QString &text, bool isSelected) const
{
    painter->save();
    
    // Set font
    painter->setFont(option.font);
    
    // Calculate text rect with margin
    QRect textRect = option.rect.adjusted(5, 0, -5, 0);
    
    // Font metrics for measuring text
    QFontMetrics fm(option.font);
    
    // Calculate proper vertical position for text baseline
    int yPos = textRect.top() + fm.ascent() + (textRect.height() - fm.height()) / 2;
    
    // Track current position
    int xPos = textRect.left();
    
    QString remainingText = text;
    
    // Find and highlight keywords
    while (!remainingText.isEmpty() && xPos < textRect.right()) {
        int earliestPos = -1;
        QString foundKeyword;
        
        // Find the earliest keyword in the remaining text (case-insensitive)
        for (const QString &keyword : m_keywords) {
            int pos = remainingText.indexOf(keyword, 0, Qt::CaseInsensitive);
            if (pos != -1 && (earliestPos == -1 || pos < earliestPos)) {
                earliestPos = pos;
                foundKeyword = keyword;
            }
        }
        
        if (earliestPos == -1) {
            // No more keywords, draw remaining text normally
            QString textToDraw = fm.elidedText(remainingText, Qt::ElideRight, textRect.right() - xPos);
            painter->setPen(isSelected ? option.palette.highlightedText().color() : option.palette.text().color());
            painter->drawText(xPos, yPos, textToDraw);
            break;
        }
        
        // Draw text before keyword
        if (earliestPos > 0) {
            QString beforeText = remainingText.left(earliestPos);
            int textWidth = fm.horizontalAdvance(beforeText);
            
            if (xPos + textWidth > textRect.right()) {
                // Not enough space, draw what fits
                beforeText = fm.elidedText(beforeText, Qt::ElideRight, textRect.right() - xPos);
                painter->setPen(isSelected ? option.palette.highlightedText().color() : option.palette.text().color());
                painter->drawText(xPos, yPos, beforeText);
                break;
            }
            
            painter->setPen(isSelected ? option.palette.highlightedText().color() : option.palette.text().color());
            painter->drawText(xPos, yPos, beforeText);
            xPos += textWidth;
        }
        
        // Draw highlighted keyword
        QString keywordText = remainingText.mid(earliestPos, foundKeyword.length());
        int keywordWidth = fm.horizontalAdvance(keywordText);
        
        if (xPos + keywordWidth > textRect.right()) {
            // Not enough space for keyword
            keywordText = fm.elidedText(keywordText, Qt::ElideRight, textRect.right() - xPos);
            keywordWidth = fm.horizontalAdvance(keywordText);
        }
        
        // Draw highlight background
        QRect highlightRect(xPos, textRect.top(), keywordWidth, textRect.height());
        painter->fillRect(highlightRect, m_colors[foundKeyword]);
        
        // Draw keyword text (darker color for better contrast)
        painter->setPen(Qt::black);
        painter->drawText(xPos, yPos, keywordText);
        xPos += keywordWidth;
        
        // Move to remaining text
        remainingText = remainingText.mid(earliestPos + foundKeyword.length());
        
        // Check if text was elided (compare lengths since case might differ)
        if (keywordText.length() != foundKeyword.length()) {
            // Text was elided, stop here
            break;
        }
    }
    
    painter->restore();
}

QSize HighlightDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    // Use default size hint
    return QStyledItemDelegate::sizeHint(option, index);
}
