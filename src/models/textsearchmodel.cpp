#include "textsearchmodel.h"

#include "colorscheme.h"

#include <algorithm>
#include <utility>

namespace {
/** Characters of a long line kept before its first match, for context. */
constexpr int kContextBeforeMatch = 30;
/** Longest preview kept per line; the view elides whatever does not fit. */
constexpr int kMaxPreviewLength = 240;
} // namespace

TextSearchModel::TextSearchModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    // Line numbers use the theme's muted colour, so repaint them on a switch.
    connect(&ColorScheme::instance(), &ColorScheme::modeChanged, this, [this]() {
        if (!m_results.isEmpty())
            emit dataChanged(index(0, LineColumn), index(rowCount() - 1, LineColumn),
                             {Qt::ForegroundRole});
    });
}

void TextSearchModel::setResults(const QString &needle, QVector<Result> results,
                                 int matchCount, bool truncated)
{
    beginResetModel();
    m_needle     = needle;
    m_results    = std::move(results);
    m_matchCount = matchCount;
    m_truncated  = truncated;
    endResetModel();
}

int TextSearchModel::rowForLine(int line) const
{
    if (line < 0)
        return -1;
    const auto it = std::lower_bound(m_results.cbegin(), m_results.cend(), line,
                                     [](const Result &result, int wanted) {
                                         return result.line < wanted;
                                     });
    if (it == m_results.cend() || it->line != line)
        return -1;
    return int(it - m_results.cbegin());
}

QString TextSearchModel::preview(const QString &line, int column)
{
    // Indentation is noise in a result list.
    qsizetype start = 0;
    while (start < line.size() && line.at(start).isSpace())
        ++start;

    QString text;
    if (column - start > kContextBeforeMatch) {
        // A long line starts a little before its match, so the match stays in view.
        start = column - kContextBeforeMatch;
        text = QStringLiteral("…");
    }
    text += QStringView(line).mid(start, kMaxPreviewLength);
    return text;
}

int TextSearchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_results.size());
}

int TextSearchModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant TextSearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.size())
        return {};
    const Result &result = m_results.at(index.row());

    if (index.column() == LineColumn) {
        switch (role) {
        case Qt::DisplayRole:
            return QString::number(result.line + 1);
        case Qt::TextAlignmentRole:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        case Qt::ForegroundRole:
            return ColorScheme::instance().mutedText();
        default:
            return {};
        }
    }

    switch (role) {
    case Qt::DisplayRole:
        return result.preview;
    case Qt::ToolTipRole:
        if (result.count > 1)
            return tr("%1 matches on line %2").arg(result.count).arg(result.line + 1);
        return {};
    default:
        return {};
    }
}
