// UiManager: log/settings/properties filter wiring, keyword highlighting and
// filter-history completers.
//
// Defines: onFilterChanged, onHighlightChanged, navigateHighlight,
// onHighlightNextClicked, onHighlightPrevClicked, updateFilterHighlighting,
// onSettingsFilterChanged, onPropertiesFilterChanged, buildFilterCriteria,
// applyFilters, passesFilter, updateFilterCount, setupFilterCompleters.
#include "uimanager.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "configurationcontroller.h"
#include "logfiltercontroller.h"
#include "historymanager.h"
#include "logmodel.h"
#include "marklogmodel.h"
#include "propertiesmodel.h"
#include "settingsmodel.h"

#include <QCompleter>
#include <QEvent>
#include <QHeaderView>
#include <QLineEdit>
#include <QScrollBar>
#include <QStatusBar>
#include <QStringListModel>
#include <QTableView>

#include <algorithm>

namespace {

/**
 * Split a filter expression into its individual keywords.
 *
 * Both `&&` and `||` are accepted and treated alike here: highlighting paints
 * every term the user typed regardless of how they are combined for matching.
 */
QStringList splitKeywords(const QString &expression)
{
    QStringList keywords;
    if (expression.isEmpty())
        return keywords;

    for (const QString &orPart : expression.split(QLatin1String("||"), Qt::SkipEmptyParts)) {
        for (const QString &andPart : orPart.split(QLatin1String("&&"), Qt::SkipEmptyParts)) {
            const QString keyword = andPart.trimmed();
            if (!keyword.isEmpty())
                keywords.append(keyword);
        }
    }
    keywords.removeDuplicates();
    return keywords;
}

/** True when any keyword appears in the entry's message, tag or package. */
bool rowMatchesKeywords(const LogEntry &entry, const QStringList &keywords)
{
    return std::any_of(keywords.cbegin(), keywords.cend(), [&entry](const QString &keyword) {
        return entry.message.contains(keyword, Qt::CaseInsensitive)
               || entry.tag.contains(keyword, Qt::CaseInsensitive)
               || entry.package.contains(keyword, Qt::CaseInsensitive);
    });
}

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
    const QStringList keywords = splitKeywords(m_ui->txtHighlight->text().trimmed());
    const QVector<LogEntry> &filtered = activeFilteredLogs();
    if (keywords.isEmpty() || filtered.isEmpty())
        return;

    const int count = filtered.size();
    // Start one step past the current hit, wrapping in the chosen direction.
    const int start = m_highlightRow < 0
                          ? (direction > 0 ? 0 : count - 1)
                          : ((m_highlightRow + direction) % count + count) % count;

    for (int step = 0; step < count; ++step) {
        const int row = ((start + direction * step) % count + count) % count;
        if (!rowMatchesKeywords(filtered.at(row), keywords))
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
    result.message = splitKeywords(inputs.message);
    result.tag     = splitKeywords(inputs.tag);
    result.package = splitKeywords(inputs.package);
    result.pid     = splitKeywords(inputs.pid);

    // The highlight box and the free-text keyword box illuminate all three
    // text columns, on top of each column's own filter terms.
    QStringList shared = splitKeywords(inputs.highlight);
    shared.append(splitKeywords(inputs.keyword));
    shared.removeDuplicates();

    for (QStringList *columnKeywords : {&result.message, &result.tag, &result.package}) {
        columnKeywords->append(shared);
        columnKeywords->removeDuplicates();
    }
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

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Configuration-tab filters
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onSettingsFilterChanged()
{
    const QString name  = m_ui->txtFilterSettings->text();
    const QString value = m_ui->txtFilterSettingsValue->text();

    m_historyManager->saveHistory(QStringLiteral("settingsKey"),   name);
    m_historyManager->saveHistory(QStringLiteral("settingsValue"), value);

    m_settingsModel->applyFilter(name, value);
}

void UiManager::onPropertiesFilterChanged()
{
    const QString name  = m_ui->txtFilterProperties->text();
    const QString value = m_ui->txtFilterPropertiesValue->text();

    m_historyManager->saveHistory(QStringLiteral("propertiesKey"),   name);
    m_historyManager->saveHistory(QStringLiteral("propertiesValue"), value);

    m_propertiesModel->applyFilter(name, value);
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
    // Persist current filter inputs to history (debounced inside HistoryManager).
    m_historyManager->saveHistory(QStringLiteral("keyword"),     m_ui->txtKeyword->text());
    m_historyManager->saveHistory(QStringLiteral("findMessage"), m_ui->txtFindMessage->text());
    m_historyManager->saveHistory(QStringLiteral("tag"),         m_ui->txtTagFilter->text());
    m_historyManager->saveHistory(QStringLiteral("pid"),         m_ui->txtPidFilter->text());
    m_historyManager->saveHistory(QStringLiteral("package"),     m_ui->txtPackageFilter->text());

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
        { m_ui->txtKeyword,                QStringLiteral("keyword")          },
        { m_ui->txtFindMessage,            QStringLiteral("findMessage")      },
        { m_ui->txtTagFilter,              QStringLiteral("tag")              },
        { m_ui->txtPackageFilter,          QStringLiteral("package")          },
        { m_ui->txtPidFilter,              QStringLiteral("pid")              },
        { m_ui->txtFilterSettings,         QStringLiteral("settingsKey")      },
        { m_ui->txtFilterSettingsValue,    QStringLiteral("settingsValue")    },
        { m_ui->txtFilterProperties,       QStringLiteral("propertiesKey")    },
        { m_ui->txtFilterPropertiesValue,  QStringLiteral("propertiesValue")  },
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
