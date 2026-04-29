// UiManager: filter chip / filter engine wiring + status updates.
// Defines: onFilterChanged, onHighlightChanged, onHighlightNextClicked, onHighlightPrevClicked,
// updateFilterHighlighting, onSettingsFilterChanged, onPropertiesFilterChanged,
// applyFilters, updateFilterCount.
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

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Filter & Highlight
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::onFilterChanged()
{
    applyFilters();
    updateFilterHighlighting();
}

void UiManager::onHighlightChanged()
{
    m_highlightRow = -1; // reset navigation on keyword change
    updateFilterHighlighting();
}

static bool rowMatchesKeywords(const LogEntry &entry, const QStringList &kws)
{
    for (const QString &kw : kws) {
        if (kw.isEmpty()) continue;
        if (entry.message.contains(kw, Qt::CaseInsensitive) ||
            entry.tag.contains(kw, Qt::CaseInsensitive) ||
            entry.package.contains(kw, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void UiManager::onHighlightNextClicked()
{
    const QString text = m_ui->txtHighlight->text().trimmed();
    auto &filtered = activeFilteredLogs();
    if (text.isEmpty() || filtered.isEmpty()) return;

    QStringList kws;
    for (const QString &p : text.split("||", Qt::SkipEmptyParts))
        for (const QString &q : p.split("&&", Qt::SkipEmptyParts)) {
            const QString kw = q.trimmed();
            if (!kw.isEmpty()) kws << kw;
        }
    if (kws.isEmpty()) return;

    const int n     = filtered.size();
    const int start = (m_highlightRow + 1) % n;
    for (int i = 0; i < n; ++i) {
        const int row = (start + i) % n;
        if (rowMatchesKeywords(filtered[row], kws)) {
            m_highlightRow     = row;
            m_pendingCenterRow = row;
            activeTableLog()->selectRow(row);
            m_rowResizeTimer->start();
            m_ui->statusbar->showMessage(
                QString("Highlight: row %1 of %2").arg(row + 1).arg(n), 2000);
            return;
        }
    }
    m_ui->statusbar->showMessage("No highlight match found", 2000);
}

void UiManager::onHighlightPrevClicked()
{
    const QString text = m_ui->txtHighlight->text().trimmed();
    auto &filtered = activeFilteredLogs();
    if (text.isEmpty() || filtered.isEmpty()) return;

    QStringList kws;
    for (const QString &p : text.split("||", Qt::SkipEmptyParts))
        for (const QString &q : p.split("&&", Qt::SkipEmptyParts)) {
            const QString kw = q.trimmed();
            if (!kw.isEmpty()) kws << kw;
        }
    if (kws.isEmpty()) return;

    const int n     = filtered.size();
    const int start = (m_highlightRow <= 0 ? n : m_highlightRow) - 1;
    for (int i = 0; i < n; ++i) {
        const int row = ((start - i) % n + n) % n;
        if (rowMatchesKeywords(filtered[row], kws)) {
            m_highlightRow     = row;
            m_pendingCenterRow = row;
            activeTableLog()->selectRow(row);
            m_rowResizeTimer->start();
            m_ui->statusbar->showMessage(
                QString("Highlight: row %1 of %2").arg(row + 1).arg(n), 2000);
            return;
        }
    }
    m_ui->statusbar->showMessage("No highlight match found", 2000);
}

void UiManager::updateFilterHighlighting()
{
    auto extractKeywords = [](const QString &filterText) -> QStringList {
        QStringList keywords;
        if (filterText.isEmpty()) return keywords;
        for (const QString &orPart : filterText.split("||", Qt::SkipEmptyParts))
            for (QString kw : orPart.split("&&", Qt::SkipEmptyParts)) {
                kw = kw.trimmed();
                if (!kw.isEmpty()) keywords.append(kw);
            }
        keywords.removeDuplicates();
        return keywords;
    };

    QStringList messageKeywords = extractKeywords(m_ui->txtFindMessage->text());
    QStringList tagKeywords     = extractKeywords(m_ui->txtTagFilter->text());
    QStringList packageKeywords = extractKeywords(m_ui->txtPackageFilter->text());
    QStringList pidKeywords     = extractKeywords(m_ui->txtPidFilter->text());

    // Highlight and keyword-filter words also illuminate all three text columns
    QStringList highlightKeywords = extractKeywords(m_ui->txtHighlight->text());
    highlightKeywords.append(extractKeywords(m_ui->txtKeyword->text()));
    highlightKeywords.removeDuplicates();

    messageKeywords.append(highlightKeywords); messageKeywords.removeDuplicates();
    tagKeywords.append(highlightKeywords);     tagKeywords.removeDuplicates();
    packageKeywords.append(highlightKeywords); packageKeywords.removeDuplicates();

    pidKeywords.isEmpty()     ? m_pidHighlightDelegate->clearKeywords()
                              : m_pidHighlightDelegate->setKeywords(pidKeywords);
    packageKeywords.isEmpty() ? m_packageHighlightDelegate->clearKeywords()
                              : m_packageHighlightDelegate->setKeywords(packageKeywords);
    tagKeywords.isEmpty()     ? m_tagHighlightDelegate->clearKeywords()
                              : m_tagHighlightDelegate->setKeywords(tagKeywords);
    messageKeywords.isEmpty() ? m_messageHighlightDelegate->clearKeywords()
                              : m_messageHighlightDelegate->setKeywords(messageKeywords);

    // Pane B has its own delegates. Mirror keywords into them only when:
    //   - sync is ON (both panes share widget state), OR
    //   - pane B is currently the active pane (the widgets reflect B's state),
    //     which we detect by m_lastActiveIsB or controller activeIsB.
    // When pane A is active and sync is OFF, leave pane B's delegates alone so
    // its highlighting reflects the snapshot from when it was last active.
    const bool paneBExists = m_logSplitController && m_logSplitController->paneB()
                             && m_logSplitController->paneB()->table;
    const bool activeIsB   = m_logSplitController && m_logSplitController->activeIsB();
    const bool updatePaneB = paneBExists && (m_syncPanes || activeIsB);
    if (updatePaneB && m_pidHighlightDelegateB) {
        pidKeywords.isEmpty()     ? m_pidHighlightDelegateB->clearKeywords()
                                  : m_pidHighlightDelegateB->setKeywords(pidKeywords);
        packageKeywords.isEmpty() ? m_packageHighlightDelegateB->clearKeywords()
                                  : m_packageHighlightDelegateB->setKeywords(packageKeywords);
        tagKeywords.isEmpty()     ? m_tagHighlightDelegateB->clearKeywords()
                                  : m_tagHighlightDelegateB->setKeywords(tagKeywords);
        messageKeywords.isEmpty() ? m_messageHighlightDelegateB->clearKeywords()
                                  : m_messageHighlightDelegateB->setKeywords(messageKeywords);
    }
    // When sync is OFF and active pane is A, the widgets show A's keywords
    // \u2014 but the per-column delegates above are pane A's. Pane A's delegates
    // should reflect A's keywords; that's already the case. When sync is OFF
    // and active pane is B, the widgets show B's keywords, so pane A's
    // delegates above are wrong \u2014 restore them from m_paneAInputs.
    if (!m_syncPanes && paneBExists && activeIsB) {
        auto kw = [&extractKeywords](const QString &t) { return extractKeywords(t); };
        QStringList aMsg  = kw(m_paneAInputs.message);
        QStringList aTag  = kw(m_paneAInputs.tag);
        QStringList aPkg  = kw(m_paneAInputs.package);
        QStringList aPid  = kw(m_paneAInputs.pid);
        QStringList aHi   = kw(m_paneAInputs.highlight);
        aHi.append(kw(m_paneAInputs.keyword));
        aHi.removeDuplicates();
        aMsg.append(aHi); aMsg.removeDuplicates();
        aTag.append(aHi); aTag.removeDuplicates();
        aPkg.append(aHi); aPkg.removeDuplicates();
        aPid.isEmpty() ? m_pidHighlightDelegate->clearKeywords()
                       : m_pidHighlightDelegate->setKeywords(aPid);
        aPkg.isEmpty() ? m_packageHighlightDelegate->clearKeywords()
                       : m_packageHighlightDelegate->setKeywords(aPkg);
        aTag.isEmpty() ? m_tagHighlightDelegate->clearKeywords()
                       : m_tagHighlightDelegate->setKeywords(aTag);
        aMsg.isEmpty() ? m_messageHighlightDelegate->clearKeywords()
                       : m_messageHighlightDelegate->setKeywords(aMsg);
    }

    m_ui->tableLog->viewport()->update();
    if (paneBExists)
        m_logSplitController->paneB()->table->viewport()->update();
}

void UiManager::onSettingsFilterChanged()
{
    const QString name  = m_ui->txtFilterSettings->text();
    const QString value = m_ui->txtFilterSettingsValue->text();

    m_historyManager->saveHistory(QStringLiteral("settingsKey"),   name);
    m_historyManager->saveHistory(QStringLiteral("settingsValue"), value);

    m_settingsModel->applyFilter(name, value);
    m_configurationController->recreateSettingsButtons();
}

void UiManager::onPropertiesFilterChanged()
{
    const QString name  = m_ui->txtFilterProperties->text();
    const QString value = m_ui->txtFilterPropertiesValue->text();

    m_historyManager->saveHistory(QStringLiteral("propertiesKey"),   name);
    m_historyManager->saveHistory(QStringLiteral("propertiesValue"), value);

    m_propertiesModel->applyFilter(name, value);
    m_configurationController->recreatePropertiesButtons();
}

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

    auto applyOnce = [this]() {
        QVector<LogEntry>&  all       = activeAllLogs();
        QVector<LogEntry>&  filtered  = activeFilteredLogs();
        QHash<quint64,int>& filtIdx   = activeFilteredLogsIndex();
        QSet<int>&          marked    = activeMarkedRows();
        LogModel*           logModel  = activeLogModel();
        MarkLogModel*       markModel = activeMarkLogModel();

        auto result = m_logFilterController->apply(all, m_logFilterController->buildCriteria());
        filtered = std::move(result.filtered);
        filtIdx  = std::move(result.index);

        logModel->setLogs(filtered);

        // Rebuild marked rows to reflect which marked logs are visible.
        marked.clear();
        const int markedCount = markModel->getMarkedCount();
        for (int i = 0; i < markedCount; ++i) {
            const int allLogsIndex = markModel->getOriginalIndex(i);
            const int filteredRow  = findLogInFilteredLogs(allLogsIndex);
            if (filteredRow >= 0) marked.insert(filteredRow);
        }
        logModel->setMarkedRows(&marked);
    };

    // Active pane.
    applyOnce();

    // Sync mode: also drive the inactive pane (only when split is active and
    // we aren't already nested inside an override-driven call).
    if (m_syncPanes && m_paneOverride < 0
        && m_logSplitController && m_logSplitController->isSplit()) {
        const bool other = !m_logSplitController->activeIsB();
        m_paneOverride = other ? 1 : 0;
        applyOnce();
        m_paneOverride = -1;
    }

    updateFilterHighlighting();
    updateFilterCount();
    updateStatusBar();

    if (m_rowResizeTimer) m_rowResizeTimer->start();
}

bool UiManager::passesFilter(const LogEntry &entry)
{
    return m_logFilterController->passesCurrent(entry);
}

void UiManager::updateFilterCount()
{
    m_ui->lblFilterCount->setText(
        QString("Showing: %1 / %2").arg(activeFilteredLogs().size()).arg(activeAllLogs().size()));
}

namespace {
// Event filter that shows the QCompleter popup when a QLineEdit gains focus
// and refreshes the history model from HistoryManager at that moment.
class FilterFocusHelper : public QObject {
public:
    FilterFocusHelper(QLineEdit *edit, HistoryManager *hm,
                      const QString &key, QStringListModel *model, QObject *parent)
        : QObject(parent), m_edit(edit), m_hm(hm), m_key(key), m_model(model) {}
protected:
    bool eventFilter(QObject *obj, QEvent *ev) override {
        if (obj == m_edit && ev->type() == QEvent::FocusIn) {
            QStringList h = m_hm->loadHistory(m_key);
            std::reverse(h.begin(), h.end());
            m_model->setStringList(h);
            if (!h.isEmpty()) {
                if (auto *c = m_edit->completer())
                    c->complete();
            }
        }
        return QObject::eventFilter(obj, ev);
    }
private:
    QLineEdit        *m_edit;
    HistoryManager   *m_hm;
    QString           m_key;
    QStringListModel *m_model;
};
} // namespace

void UiManager::setupFilterCompleters()
{
    struct FilterSpec {
        QLineEdit  *edit;
        QString     historyKey;
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

    for (const auto &spec : specs) {
        if (!spec.edit) continue;

        auto *model     = new QStringListModel(spec.edit);
        auto *completer = new QCompleter(model, spec.edit);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        spec.edit->setCompleter(completer);

        // Refresh model on text change so the dropdown stays in sync.
        const QString key = spec.historyKey;
        connect(spec.edit, &QLineEdit::textChanged, this,
                [this, model, key](const QString &) {
            QStringList h = m_historyManager->loadHistory(key);
            std::reverse(h.begin(), h.end());
            model->setStringList(h);
        });

        // Load initial history.
        QStringList initial = m_historyManager->loadHistory(key);
        std::reverse(initial.begin(), initial.end());
        model->setStringList(initial);

        // Show the dropdown when the edit gains focus (click or tab).
        auto *helper = new FilterFocusHelper(spec.edit, m_historyManager,
                                             key, model, spec.edit);
        spec.edit->installEventFilter(helper);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Device / Logcat
// ─────────────────────────────────────────────────────────────────────────────

