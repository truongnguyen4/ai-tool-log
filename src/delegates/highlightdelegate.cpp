#include "highlightdelegate.h"
#include <QPainter>
#include <QApplication>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTableView>
#include <QHeaderView>

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

void HighlightDelegate::paintMarkedBackground(QPainter *painter,
                                              const QStyleOptionViewItem &opt,
                                              const QModelIndex &index) const
{
    // Qt's stylesheet engine ignores Qt::BackgroundRole when any ::item
    // background-color rule exists in the QSS.  We read it explicitly and
    // paint it here — AFTER style->drawControl — so it sits on top of the
    // stylesheet's normal/hover fill but below keyword spans and text.
    if (opt.state & QStyle::State_Selected)
        return;   // selection highlight takes precedence; don't overlay
    const QVariant bg = index.data(Qt::BackgroundRole);
    if (!bg.isValid()) return;
    QColor bgColor = bg.value<QColor>();
    if (!bgColor.isValid()) return;
    bgColor.setAlpha(210);
    painter->fillRect(opt.rect, bgColor);
}

void HighlightDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    // ── No keywords ────────────────────────────────────────────────────────
    if (m_keywords.isEmpty()) {
        if (!m_wordWrap) {
            // QStyledItemDelegate::paint() calls style->drawControl() which ignores
            // Qt::BackgroundRole when a QSS ::item rule is active.  Draw manually.
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);
            const QString text = opt.text;
            opt.text.clear();
            const QWidget *widget = opt.widget;
            QStyle *style = widget ? widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
            paintMarkedBackground(painter, opt, index);  // overlay marked row color
            painter->save();
            painter->setFont(opt.font);
            const bool sel = opt.state & QStyle::State_Selected;
            painter->setPen(sel ? opt.palette.highlightedText().color()
                                : opt.palette.text().color());
            painter->drawText(opt.rect.adjusted(6, 0, -6, 0),
                              Qt::AlignLeft | Qt::AlignVCenter, text);
            painter->restore();
            return;
        }
        // Word-wrap, no highlights
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        const QString text = opt.text;
        opt.text.clear();
        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
        paintMarkedBackground(painter, opt, index);  // overlay marked row color

        painter->save();
        painter->setFont(opt.font);
        const bool sel = opt.state & QStyle::State_Selected;
        painter->setPen(sel ? opt.palette.highlightedText().color()
                            : opt.palette.text().color());
        painter->drawText(opt.rect.adjusted(5, 3, -5, -3),
                          Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, text);
        painter->restore();
        return;
    }

    // ── Has keywords ────────────────────────────────────────────────────────
    QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    opt.text = "";
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    paintMarkedBackground(painter, opt, index);  // overlay marked row color

    const bool isSelected = option.state & QStyle::State_Selected;

    if (!m_wordWrap) {
        // Single-line highlight path (original behaviour)
        drawHighlightedText(painter, option, text, isSelected);
        return;
    }

    // Word-wrap + keyword highlights: render via QTextDocument with HTML spans
    QString html = text.toHtmlEscaped();
    for (const QString &kw : m_keywords) {
        const QColor &c = m_colors[kw];
        const QString spanOpen = QString("<span style=\"background-color:%1;color:black;\">").arg(c.name());
        const QString kwEsc = kw.toHtmlEscaped();
        QString result;
        int pos = 0;
        while (pos < html.size()) {
            int found = html.indexOf(kwEsc, pos, Qt::CaseInsensitive);
            if (found == -1) { result += html.mid(pos); break; }
            result += html.mid(pos, found - pos);
            result += spanOpen + html.mid(found, kwEsc.size()) + "</span>";
            pos = found + kwEsc.size();
        }
        html = result;
    }
    const QString textColor = isSelected ? opt.palette.highlightedText().color().name()
                                         : opt.palette.text().color().name();
    QTextDocument doc;
    doc.setDefaultFont(opt.font);
    doc.setHtml("<span style=\"color:" + textColor + ";white-space:pre-wrap;\">" + html + "</span>");
    doc.setTextWidth(opt.rect.width() - 10);

    painter->save();
    const int docHeight = static_cast<int>(doc.size().height());
    const int yOffset = qMax(0, (opt.rect.height() - docHeight) / 2);
    painter->translate(opt.rect.left() + 5, opt.rect.top() + yOffset);
    QAbstractTextDocumentLayout::PaintContext ctx;
    ctx.palette = opt.palette;
    doc.documentLayout()->draw(painter, ctx);
    painter->restore();
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
    if (!m_wordWrap)
        return QStyledItemDelegate::sizeHint(option, index);

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // When called from resizeRowToContents() / sizeHintForRow(), Qt sets
    // opt.rect to the full view rect (via initViewItemOption), NOT the cell
    // rect. Always read the actual column width from the view so the
    // word-wrap height is computed against the real available space.
    int availWidth = 400;
    if (auto *tv = qobject_cast<const QTableView *>(opt.widget)) {
        int cw = tv->columnWidth(index.column());
        if (cw > 10) availWidth = cw;
    }
    availWidth -= 10;      // cell padding

    const QFontMetrics fm(opt.font);
    const QRect br = fm.boundingRect(QRect(0, 0, availWidth, 0),
                                      Qt::TextWordWrap | Qt::AlignLeft,
                                      opt.text);
    return QSize(availWidth + 10, qMax(br.height() + 6, fm.height() + 6));
}

void HighlightDelegate::setWordWrap(bool enabled)
{
    m_wordWrap = enabled;
}
