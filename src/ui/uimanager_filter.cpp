// UiManager: log/settings/properties filter wiring, keyword highlighting and
// filter-history completers.
//
// Defines: onFilterChanged, onHighlightChanged, navigateHighlight,
// onHighlightNextClicked, onHighlightPrevClicked, updateFilterHighlighting,
// updateQueryHint, onSettingsFilterChanged, onPropertiesFilterChanged,
// updateSettingsFilterStatus, updatePropertiesFilterStatus,
// showTableFilterStatus, buildFilterCriteria, applyFilters, passesFilter,
// updateFilterCount, setupFilterCompleters.
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "configurationcontroller.h"
#include "logfiltercontroller.h"
#include "logquery.h"
#include "tablequery.h"
#include "historymanager.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "propertiesmodel.h"
#include "settingsmodel.h"

#include <QCompleter>
#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QStatusBar>
#include <QStringListModel>
#include <QStyle>
#include <QTableView>

#include <algorithm>

namespace {

/** Apply keywords to a delegate, clearing it when the list is empty. */
void setDelegateKeywords(HighlightDelegate *delegate, const QStringList &keywords)
{
    if (!delegate)
        return;
    if (keywords.isEmpty())
        delegate->clearKeywords();
    else
        delegate->setKeywords(keywords);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Filter & Highlight
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onFilterChanged()
{
    // applyFilters() refreshes the highlighting itself; calling it again here
    // would repaint every visible row a second time for no benefit.
    applyFilters();
}

void UiManager::onHighlightChanged()
{
    m_highlightRow = -1; // restart navigation whenever the keyword changes
    updateFilterHighlighting();
}

void UiManager::navigateHighlight(int direction)
{
    // Stop on every row the highlight paints: a row holding any of the terms,
    // not a row the box would pass if it were a filter.
    const QVector<LogQuery::Term> terms =
        LogQuery::parse(m_ui->txtHighlight->text()).positiveTerms();
    const QVector<LogEntry> &filtered = activeFilteredLogs();
    if (terms.isEmpty() || filtered.isEmpty())
        return;

    const auto rowMatches = [&terms](const LogEntry &entry) {
        return std::any_of(terms.cbegin(), terms.cend(), [&entry](const LogQuery::Term &term) {
            return LogQuery::termMatches(term, entry);
        });
    };

    const int count = filtered.size();
    // Start one step past the current hit, wrapping in the chosen direction.
    const int start = m_highlightRow < 0
                          ? (direction > 0 ? 0 : count - 1)
                          : ((m_highlightRow + direction) % count + count) % count;

    for (int step = 0; step < count; ++step) {
        const int row = ((start + direction * step) % count + count) % count;
        if (!rowMatches(filtered.at(row)))
            continue;

        m_highlightRow     = row;
        m_pendingCenterRow = row;
        if (QTableView *table = activeTableLog())
            table->selectRow(row);
        m_rowResizeTimer->start();
        flashStatus(tr("Highlight: row %1 of %2").arg(row + 1).arg(count));
        return;
    }

    flashStatus(tr("No highlight match found"));
}

void UiManager::onHighlightNextClicked() { navigateHighlight(+1); }
void UiManager::onHighlightPrevClicked() { navigateHighlight(-1); }

UiManager::HighlightKeywords UiManager::collectHighlightKeywords(const PaneInputs &inputs) const
{
    HighlightKeywords result;

    // Both boxes paint what they look for: a bare word lights up Tag and
    // Message, a column term only its own column. Terms under a "-" are left
    // out — they name what the user does not want to see.
    const auto addTerms = [&result](const QString &text) {
        const QVector<LogQuery::Term> terms = LogQuery::parse(text).positiveTerms();
        for (const LogQuery::Term &term : terms) {
            switch (term.field) {
            case LogQuery::Field::Any:
                result.tag.append(term.text);
                result.message.append(term.text);
                break;
            case LogQuery::Field::Tag:     result.tag.append(term.text);     break;
            case LogQuery::Field::Message: result.message.append(term.text); break;
            case LogQuery::Field::Package: result.package.append(term.text); break;
            case LogQuery::Field::Pid:     result.pid.append(term.text);     break;
            case LogQuery::Field::Tid:     break;   // the TID column has no highlight delegate
            }
        }
    };
    addTerms(inputs.query);
    addTerms(inputs.highlight);

    for (QStringList *columnKeywords : {&result.message, &result.tag, &result.package, &result.pid})
        columnKeywords->removeDuplicates();
    return result;
}

void UiManager::applyHighlightKeywords(const HighlightKeywords &keywords, bool paneB)
{
    setDelegateKeywords(paneB ? m_pidHighlightDelegateB     : m_pidHighlightDelegate,     keywords.pid);
    setDelegateKeywords(paneB ? m_packageHighlightDelegateB : m_packageHighlightDelegate, keywords.package);
    setDelegateKeywords(paneB ? m_tagHighlightDelegateB     : m_tagHighlightDelegate,     keywords.tag);
    setDelegateKeywords(paneB ? m_messageHighlightDelegateB : m_messageHighlightDelegate, keywords.message);
}

void UiManager::updateFilterHighlighting()
{
    // The filter widgets always describe the *active* pane. The inactive pane
    // keeps the keywords captured when it was last active, which is why each
    // pane owns its own set of delegates.
    PaneInputs liveInputs;
    snapshotInputsTo(liveInputs);
    const HighlightKeywords liveKeywords = collectHighlightKeywords(liveInputs);

    const bool paneBExists = m_logSplitController && m_logSplitController->paneB()
                             && m_logSplitController->paneB()->table;
    const bool activeIsB   = m_logSplitController && m_logSplitController->activeIsB();

    if (m_syncPanes || !activeIsB)
        applyHighlightKeywords(liveKeywords, /*paneB=*/false);
    else
        applyHighlightKeywords(collectHighlightKeywords(m_paneAInputs), /*paneB=*/false);

    if (paneBExists && (m_syncPanes || activeIsB))
        applyHighlightKeywords(liveKeywords, /*paneB=*/true);

    m_ui->tableLog->viewport()->update();
    if (paneBExists)
        m_logSplitController->paneB()->table->viewport()->update();
}

void UiManager::updateQueryHint()
{
    QLabel *hint = m_ui->lblQueryHint;
    if (!hint)
        return;

    const LogQuery &applied = m_logFilterController->criteria().query;
    QString state;
    QString text;
    if (m_ui->txtLogQuery->text().trimmed() != applied.text().trimmed()) {
        state = QStringLiteral("pending");
        text  = tr("Press Enter to apply");
    } else if (!applied.warning().isEmpty()) {
        state = QStringLiteral("warning");
        text  = applied.warning();
    } else {
        text  = tr("Words search Tag and Message. Hover the box for syntax.");
    }

    hint->setText(text);
    if (hint->property("state").toString() != state) {
        // Colours come from the theme sheet's QLabel#lblQueryHint[state=...].
        hint->setProperty("state", state.isEmpty() ? QVariant() : QVariant(state));
        hint->style()->unpolish(hint);
        hint->style()->polish(hint);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Configuration-tab filters
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onSettingsFilterChanged()
{
    const QString query = m_ui->txtFilterSettings->text();
    m_historyManager->saveHistory(QStringLiteral("settingsQuery"), query);
    // The model reset that follows updates the row count beside the box.
    m_settingsModel->applyFilter(query);
}

void UiManager::onPropertiesFilterChanged()
{
    const QString query = m_ui->txtFilterProperties->text();
    m_historyManager->saveHistory(QStringLiteral("propertiesQuery"), query);
    m_propertiesModel->applyFilter(query);
}

void UiManager::updateSettingsFilterStatus()
{
    showTableFilterStatus(m_ui->txtFilterSettings, m_ui->lblSettingsFilterStatus,
                          m_settingsModel->filterQuery(), m_settingsModel->rowCount(),
                          int(m_settingsModel->getSettings().size()));
}

void UiManager::updatePropertiesFilterStatus()
{
    showTableFilterStatus(m_ui->txtFilterProperties, m_ui->lblPropertiesFilterStatus,
                          m_propertiesModel->filterQuery(), m_propertiesModel->rowCount(),
                          int(m_propertiesModel->getProperties().size()));
}

void UiManager::showTableFilterStatus(QLineEdit *edit, QLabel *status, const TableQuery &query,
                                      int visibleRows, int totalRows)
{
    if (!edit || !status)
        return;

    QString text = !query.isEmpty() ? tr("%1 / %2").arg(visibleRows).arg(totalRows)
                   : totalRows == 1 ? tr("1 row")
                                    : tr("%1 rows").arg(totalRows);
    const bool warning = !query.warning().isEmpty();
    if (warning)
        text.prepend(QStringLiteral("⚠ "));
    status->setText(text);
    // The box's own tooltip is the syntax help, so the repair note goes here.
    status->setToolTip(warning ? query.warning() : QString());

    // Warning colours come from the theme sheet's [state="warning"] rules.
    const QVariant state = warning ? QVariant(QStringLiteral("warning")) : QVariant();
    for (QWidget *widget : {static_cast<QWidget *>(edit), static_cast<QWidget *>(status)}) {
        if (widget->property("state") == state)
            continue;
        widget->setProperty("state", state);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Log filtering
// ─────────────────────────────────────────────────────────────────────────────

FilterCriteria UiManager::buildFilterCriteria() const
{
    return m_logFilterController->buildCriteria();
}

void UiManager::applyFilters()
{
    // Persist the filter to history (debounced inside HistoryManager).
    m_historyManager->saveHistory(QStringLiteral("logQuery"), m_ui->txtLogQuery->text());

    // Build the criteria once and reuse it for both panes; it is also cached
    // on the controller so per-line ingestion doesn't have to rebuild it.
    const FilterCriteria criteria = m_logFilterController->refreshCriteria();

    auto applyToActivePane = [this, &criteria]() {
        QVector<LogEntry>  &all       = activeAllLogs();
        QVector<LogEntry>  &filtered  = activeFilteredLogs();
        QHash<quint64,int> &filtIdx   = activeFilteredLogsIndex();
        QSet<int>          &marked    = activeMarkedRows();
        LogModel           *logModel  = activeLogModel();
        MarkLogModel       *markModel = activeMarkLogModel();

        auto result = m_logFilterController->apply(all, criteria);
        filtered = std::move(result.filtered);
        filtIdx  = std::move(result.index);

        logModel->setLogs(filtered);

        // Rebuild the marked-row set to reflect which marks are still visible.
        marked.clear();
        const int markedCount = markModel->getMarkedCount();
        for (int i = 0; i < markedCount; ++i) {
            const int filteredRow = findLogInFilteredLogs(markModel->getOriginalIndex(i));
            if (filteredRow >= 0)
                marked.insert(filteredRow);
        }
        logModel->setMarkedRows(&marked);
    };

    applyToActivePane();

    // Sync mode: drive the inactive pane too, unless we are already nested
    // inside an override-driven call.
    if (m_syncPanes && m_paneOverride < 0
        && m_logSplitController && m_logSplitController->isSplit()) {
        m_paneOverride = m_logSplitController->activeIsB() ? 0 : 1;
        applyToActivePane();
        m_paneOverride = -1;
    }

    updateFilterHighlighting();
    updateQueryHint();
    updateFilterCount();
    updateStatusBar();

    if (m_rowResizeTimer)
        m_rowResizeTimer->start();
}

bool UiManager::passesFilter(const LogEntry &entry)
{
    return m_logFilterController->passesCurrent(entry);
}

void UiManager::updateFilterCount()
{
    m_ui->lblFilterCount->setText(
        tr("Showing: %1 / %2").arg(activeFilteredLogs().size()).arg(activeAllLogs().size()));
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Filter-history completers
// ─────────────────────────────────────────────────────────────────────────────

namespace {
/**
 * Pops the completer open when a filter box gains focus, so the user can pick
 * a previous value without typing.
 */
class FilterFocusHelper : public QObject
{
public:
    FilterFocusHelper(QLineEdit *edit, QObject *parent)
        : QObject(parent), m_edit(edit) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_edit && event->type() == QEvent::FocusIn) {
            if (QCompleter *completer = m_edit->completer()) {
                if (completer->model() && completer->model()->rowCount() > 0)
                    completer->complete();
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QLineEdit *m_edit;
};
} // namespace

void UiManager::setupFilterCompleters()
{
    struct FilterSpec {
        QLineEdit *edit;
        QString    historyKey;
    };
    const QList<FilterSpec> specs = {
        { m_ui->txtLogQuery,          QStringLiteral("logQuery")        },
        { m_ui->txtFilterSettings,    QStringLiteral("settingsQuery")   },
        { m_ui->txtFilterProperties,  QStringLiteral("propertiesQuery") },
    };

    for (const FilterSpec &spec : specs) {
        if (!spec.edit)
            continue;

        auto *model     = new QStringListModel(spec.edit);
        auto *completer = new QCompleter(model, spec.edit);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        spec.edit->setCompleter(completer);

        const QString key = spec.historyKey;
        auto reload = [this, model, key]() {
            QStringList history = m_historyManager->loadHistory(key);
            std::reverse(history.begin(), history.end());   // most recent first
            model->setStringList(history);
        };
        reload();

        // Refresh only when the stored history actually changes. Reloading on
        // every keystroke — as this used to — rebuilt the model, and with it
        // the completer's match index, on each character typed.
        connect(m_historyManager, &HistoryManager::historyChanged, model,
                [reload, key](const QString &changedKey) {
                    if (changedKey == key)
                        reload();
                });

        spec.edit->installEventFilter(new FilterFocusHelper(spec.edit, spec.edit));
    }
}
