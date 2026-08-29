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

    criteria.query     = LogQuery::parse(m_ui->txtLogQuery->text());
    criteria.startTime = m_ui->txtStartTime->text().trimmed();
    criteria.endTime   = m_ui->txtEndTime->text().trimmed();

    // "Verbose+" and "V" both mean "no level floor"; the rest raise it.
    QString minLevel = QStringLiteral("V");
    if      (m_ui->radioD->isChecked()) minLevel = QStringLiteral("D");
    else if (m_ui->radioI->isChecked()) minLevel = QStringLiteral("I");
    else if (m_ui->radioW->isChecked()) minLevel = QStringLiteral("W");
    else if (m_ui->radioE->isChecked()) minLevel = QStringLiteral("E");
    else if (m_ui->radioA->isChecked()) minLevel = QStringLiteral("A");
    criteria.minLevelIndex = LogFilter::levelIndex(minLevel);

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
