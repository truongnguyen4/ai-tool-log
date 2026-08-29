#include "logfiltercontroller.h"
#include "ui_mainwindow.h"

#include <QtConcurrent/QtConcurrent>
#include <QLineEdit>
#include <QRadioButton>

namespace {
/**
 * Number of entries below which parallel filtering costs more in task setup
 * than it saves. Measured on a 4-core laptop; the exact figure is not
 * critical, only that tiny buffers stay on the calling thread.
 */
constexpr int kParallelFilterThreshold = 20000;
} // namespace

LogFilterController::LogFilterController(Ui::MainWindow *ui, QObject *parent)
    : QObject(parent)
    , m_ui(ui)
{}

FilterCriteria LogFilterController::buildCriteria() const
{
    FilterCriteria criteria;

    criteria.keywordFilter = m_ui->txtKeyword->text().trimmed();
    if (!criteria.keywordFilter.isEmpty()) {
        criteria.keywordRegex = QRegularExpression(criteria.keywordFilter,
                                                   QRegularExpression::CaseInsensitiveOption);
        criteria.keywordRegex.optimize();
    }

    FilterCriteria::applyFilter(criteria.messageFilter, criteria.messageOperator,
                                m_ui->txtFindMessage->text());
    FilterCriteria::applyFilter(criteria.tagFilter,     criteria.tagOperator,
                                m_ui->txtTagFilter->text());
    FilterCriteria::applyFilter(criteria.packageFilter, criteria.packageOperator,
                                m_ui->txtPackageFilter->text());
    FilterCriteria::applyFilter(criteria.pidFilter,     criteria.pidOperator,
                                m_ui->txtPidFilter->text());
    FilterCriteria::applyFilter(criteria.tidFilter,     criteria.tidOperator,
                                m_ui->txtTidFilter->text());

    criteria.startTime = m_ui->txtStartTime->text().trimmed();
    criteria.endTime   = m_ui->txtEndTime->text().trimmed();

    // "Verbose+" and "V" both mean "no level floor"; the rest raise it.
    if      (m_ui->radioD->isChecked()) criteria.minLevel = QStringLiteral("D");
    else if (m_ui->radioI->isChecked()) criteria.minLevel = QStringLiteral("I");
    else if (m_ui->radioW->isChecked()) criteria.minLevel = QStringLiteral("W");
    else if (m_ui->radioE->isChecked()) criteria.minLevel = QStringLiteral("E");
    else if (m_ui->radioA->isChecked()) criteria.minLevel = QStringLiteral("A");
    else                                criteria.minLevel = QStringLiteral("V");

    criteria.parsedMessage = ParsedFilter::build(criteria.messageFilter);
    criteria.parsedTag     = ParsedFilter::build(criteria.tagFilter);
    criteria.parsedPackage = ParsedFilter::build(criteria.packageFilter);
    criteria.parsedPid     = ParsedFilter::build(criteria.pidFilter);
    criteria.parsedTid     = ParsedFilter::build(criteria.tidFilter);

    criteria.minLevelIndex = LogFilter::levelIndex(criteria.minLevel);

    return criteria;
}

const FilterCriteria &LogFilterController::refreshCriteria()
{
    m_criteria      = buildCriteria();
    m_criteriaValid = true;
    return m_criteria;
}

const FilterCriteria &LogFilterController::criteria() const
{
    if (!m_criteriaValid) {
        m_criteria      = buildCriteria();
        m_criteriaValid = true;
    }
    return m_criteria;
}

LogFilterController::Result LogFilterController::apply(const QVector<LogEntry> &allLogs,
                                                       const FilterCriteria &criteria) const
{
    Result result;

    if (!criteria.isActive()) {
        // No constraint at all: the filtered view is the full log. QVector is
        // implicitly shared, so this is a refcount bump rather than a copy of
        // what can be millions of entries.
        result.filtered = allLogs;
    } else if (allLogs.size() < kParallelFilterThreshold) {
        result.filtered.reserve(allLogs.size());
        for (const LogEntry &entry : allLogs) {
            if (m_logFilter.passesFilter(entry, criteria))
                result.filtered.append(entry);
        }
    } else {
        result.filtered = QtConcurrent::blockingFiltered(
            allLogs, [this, &criteria](const LogEntry &entry) {
                return m_logFilter.passesFilter(entry, criteria);
            });
    }

    result.index.reserve(result.filtered.size());
    for (int row = 0; row < result.filtered.size(); ++row)
        result.index.insert(result.filtered.at(row).id, row);

    return result;
}

bool LogFilterController::passesCurrent(const LogEntry &entry) const
{
    return m_logFilter.passesFilter(entry, criteria());
}
