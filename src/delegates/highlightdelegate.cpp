#include "highlightdelegate.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QFontMetrics>
#include <QHeaderView>
#include <QPainter>
#include <QTableView>
#include <QTextDocument>

#include <algorithm>

namespace {

/** Keyword background palette — vibrant but still readable under black text. */
const QColor kKeywordColors[] = {
    QColor(255, 165, 0,   180),  // orange
    QColor(135, 206, 250, 180),  // light sky blue
    QColor(144, 238, 144, 180),  // light green
    QColor(255, 182, 193, 180),  // light pink
    QColor(221, 160, 221, 180),  // plum
    QColor(255, 255, 0,   180),  // yellow
    QColor(0,   255, 255, 180),  // cyan
    QColor(255, 192, 203, 180),  // pink
    QColor(173, 216, 230, 180),  // light blue
    QColor(152, 251, 152, 180),  // pale green
};
constexpr int kKeywordColorCount = int(std::size(kKeywordColors));

/** Cell padding, in pixels. */
constexpr int kPaddingX = 5;
constexpr int kPaddingY = 3;
/** Fallback column width when the view cannot be queried. */
constexpr int kFallbackWidth = 400;
/** Opacity applied to a marked row's background tint. */
constexpr int kMarkedRowAlpha = 210;

QColor keywordColor(int index)
{
    return kKeywordColors[index % kKeywordColorCount];
}

} // namespace

HighlightDelegate::HighlightDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void HighlightDelegate::setKeywords(const QStringList &keywords)
{
    if (m_keywords == keywords)
        return;   // nothing to repaint

    m_keywords = keywords;
    m_colors.clear();
    m_colors.reserve(keywords.size());
    for (int i = 0; i < keywords.size(); ++i)
        m_colors.insert(keywords.at(i), keywordColor(i));
}

void HighlightDelegate::clearKeywords()
{
    m_keywords.clear();
    m_colors.clear();
}

void HighlightDelegate::setWordWrap(bool enabled)
{
    m_wordWrap = enabled;
}

bool HighlightDelegate::containsAnyKeyword(const QString &text) const
{
    return std::any_of(m_keywords.cbegin(), m_keywords.cend(),
                       [&text](const QString &keyword) {
                           return text.contains(keyword, Qt::CaseInsensitive);
                       });
}

int HighlightDelegate::contentWidth(const QStyleOptionViewItem &option,
                                    const QModelIndex &index)
{
    // When Qt calls sizeHint() from resizeRowToContents(), option.rect is the
    // whole view rather than the cell, so ask the view for the real column
    // width instead of trusting the rect.
    int width = kFallbackWidth;
    if (const auto *view = qobject_cast<const QTableView *>(option.widget)) {
        const int columnWidth = view->columnWidth(index.column());
        if (columnWidth > 2 * kPaddingX)
            width = columnWidth;
    }
    return width - 2 * kPaddingX;
}

void HighlightDelegate::drawItemBackground(QPainter *painter,
                                           const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const
{
    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &option, painter, widget);

    // Qt's stylesheet engine ignores Qt::BackgroundRole once any QTableView
    // ::item background rule exists, so the marked-row tint is painted here by
    // hand — after the style, below the keyword spans and the text.
    if (option.state & QStyle::State_Selected)
        return;   // the selection colour wins
    const QVariant background = index.data(Qt::BackgroundRole);
    if (!background.isValid())
        return;
    QColor color = background.value<QColor>();
    if (!color.isValid())
        return;
    color.setAlpha(kMarkedRowAlpha);
    painter->fillRect(option.rect, color);
}

void HighlightDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const QString text = opt.text;
    opt.text.clear();               // we draw the text ourselves
    drawItemBackground(painter, opt, index);
    if (text.isEmpty())
        return;

    const bool selected = opt.state & QStyle::State_Selected;

    // The overwhelmingly common case is a cell with no keyword in it. Checking
    // first keeps those cells on the cheap plain-text path instead of building
    // an HTML string and a QTextDocument for every visible row on every paint.
    if (m_keywords.isEmpty() || !containsAnyKeyword(text)) {
        drawPlainText(painter, opt, text, selected);
        return;
    }

    if (m_wordWrap)
        drawWrappedHighlightedText(painter, opt, text, selected);
    else
        drawHighlightedText(painter, opt, text, selected);
}

void HighlightDelegate::drawPlainText(QPainter *painter, const QStyleOptionViewItem &option,
                                      const QString &text, bool selected) const
{
    painter->save();
    painter->setFont(option.font);
    painter->setPen(selected ? option.palette.highlightedText().color()
                             : option.palette.text().color());

    if (m_wordWrap) {
        painter->drawText(option.rect.adjusted(kPaddingX, kPaddingY, -kPaddingX, -kPaddingY),
                          Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, text);
    } else {
        const QRect textRect = option.rect.adjusted(kPaddingX, 0, -kPaddingX, 0);
        const QFontMetrics metrics(option.font);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(text, Qt::ElideRight, textRect.width()));
    }

    painter->restore();
}

void HighlightDelegate::drawHighlightedText(QPainter *painter,
                                            const QStyleOptionViewItem &option,
                                            const QString &text, bool selected) const
{
    painter->save();
    painter->setFont(option.font);

    const QRect textRect = option.rect.adjusted(kPaddingX, 0, -kPaddingX, 0);
    const QFontMetrics metrics(option.font);
    const int baseline = textRect.top() + metrics.ascent()
                         + (textRect.height() - metrics.height()) / 2;
    const QColor textColor = selected ? option.palette.highlightedText().color()
                                      : option.palette.text().color();

    int x = textRect.left();
    QString remaining = text;

    while (!remaining.isEmpty() && x < textRect.right()) {
        // Find the earliest keyword still ahead of us.
        int matchPos = -1;
        int matchLength = 0;
        QColor matchColor;
        for (const QString &keyword : m_keywords) {
            const int pos = remaining.indexOf(keyword, 0, Qt::CaseInsensitive);
            if (pos >= 0 && (matchPos < 0 || pos < matchPos)) {
                matchPos    = pos;
                matchLength = keyword.size();
                matchColor  = m_colors.value(keyword, keywordColor(0));
            }
        }

        if (matchPos < 0) {
            painter->setPen(textColor);
            painter->drawText(x, baseline,
                              metrics.elidedText(remaining, Qt::ElideRight,
                                                 textRect.right() - x));
            break;
        }

        if (matchPos > 0) {
            const QString before = remaining.left(matchPos);
            const int width = metrics.horizontalAdvance(before);
            painter->setPen(textColor);
            if (x + width > textRect.right()) {
                painter->drawText(x, baseline,
                                  metrics.elidedText(before, Qt::ElideRight,
                                                     textRect.right() - x));
                break;
            }
            painter->drawText(x, baseline, before);
            x += width;
        }

        // The matched span keeps the source text's own casing.
        QString match = remaining.mid(matchPos, matchLength);
        int matchWidth = metrics.horizontalAdvance(match);
        const bool elided = x + matchWidth > textRect.right();
        if (elided) {
            match = metrics.elidedText(match, Qt::ElideRight, textRect.right() - x);
            matchWidth = metrics.horizontalAdvance(match);
        }

        painter->fillRect(QRect(x, textRect.top(), matchWidth, textRect.height()),
                          matchColor);
        painter->setPen(Qt::black);   // keyword fills are light; black reads best
        painter->drawText(x, baseline, match);
        x += matchWidth;

        if (elided)
            break;
        remaining = remaining.mid(matchPos + matchLength);
    }

    painter->restore();
}

void HighlightDelegate::drawWrappedHighlightedText(QPainter *painter,
                                                   const QStyleOptionViewItem &option,
                                                   const QString &text, bool selected) const
{
    // Build one HTML fragment with a <span> per keyword occurrence and let
    // QTextDocument do the wrapping. Only reached for cells that actually
    // contain a keyword.
    QString html = text.toHtmlEscaped();
    for (const QString &keyword : m_keywords) {
        const QString needle = keyword.toHtmlEscaped();
        if (needle.isEmpty() || !html.contains(needle, Qt::CaseInsensitive))
            continue;

        const QString openTag =
            QStringLiteral("<span style=\"background-color:%1;color:black;\">")
                .arg(m_colors.value(keyword, keywordColor(0)).name());

        QString wrapped;
        wrapped.reserve(html.size() + needle.size());
        int pos = 0;
        while (pos < html.size()) {
            const int found = html.indexOf(needle, pos, Qt::CaseInsensitive);
            if (found < 0) {
                wrapped += QStringView(html).mid(pos);
                break;
            }
            wrapped += QStringView(html).mid(pos, found - pos);
            wrapped += openTag;
            wrapped += QStringView(html).mid(found, needle.size());
            wrapped += QLatin1String("</span>");
            pos = found + needle.size();
        }
        html = wrapped;
    }

    const QColor textColor = selected ? option.palette.highlightedText().color()
                                      : option.palette.text().color();
    QTextDocument document;
    document.setDefaultFont(option.font);
    document.setDocumentMargin(0);
    document.setHtml(QStringLiteral("<span style=\"color:%1;white-space:pre-wrap;\">%2</span>")
                         .arg(textColor.name(), html));
    document.setTextWidth(option.rect.width() - 2 * kPaddingX);

    painter->save();
    const int documentHeight = int(document.size().height());
    const int yOffset = qMax(0, (option.rect.height() - documentHeight) / 2);
    painter->translate(option.rect.left() + kPaddingX, option.rect.top() + yOffset);
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette = option.palette;
    document.documentLayout()->draw(painter, context);
    painter->restore();
}

QSize HighlightDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    if (!m_wordWrap)
        return QStyledItemDelegate::sizeHint(option, index);

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const int available = contentWidth(opt, index);
    const QFontMetrics metrics(opt.font);
    const QRect bounds = metrics.boundingRect(QRect(0, 0, available, 0),
                                              Qt::TextWordWrap | Qt::AlignLeft,
                                              opt.text);
    return QSize(available + 2 * kPaddingX,
                 qMax(bounds.height(), metrics.height()) + 2 * kPaddingY);
}
