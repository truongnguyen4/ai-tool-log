#ifndef TEXTSEARCHMODEL_H
#define TEXTSEARCHMODEL_H

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

/**
 * The lines of a text that contain a search term, one row per line, for a
 * VS Code-style result list: the line number, then a preview of the line with
 * its first match kept in view.
 *
 * Rows carry their own preview, so the list stays intact while the searched
 * document is being replaced; the next search pass repopulates it.
 */
class TextSearchModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { LineColumn, TextColumn, ColumnCount };

    struct Result {
        int     line   = 0;   ///< zero-based line (block) number
        int     column = 0;   ///< offset of the line's first match
        int     count  = 0;   ///< matches on the line
        QString preview;      ///< see preview()
    };

    explicit TextSearchModel(QObject *parent = nullptr);

    /** Replace the rows. @p results must be ordered by line. */
    void setResults(const QString &needle, QVector<Result> results,
                    int matchCount, bool truncated);

    const QString &needle() const        { return m_needle; }
    const Result  &result(int row) const { return m_results.at(row); }
    /** Row holding @p line, or -1 when that line has no match. */
    int  rowForLine(int line) const;
    /** Matches in the whole text, including lines past the listing limit. */
    int  matchCount() const              { return m_matchCount; }
    /** True when more lines matched than are listed. */
    bool isTruncated() const             { return m_truncated; }

    /** @p line cut down to a one-line preview that keeps @p column in view. */
    static QString preview(const QString &line, int column);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
    QString         m_needle;
    QVector<Result> m_results;
    int             m_matchCount = 0;
    bool            m_truncated  = false;
};

#endif // TEXTSEARCHMODEL_H
