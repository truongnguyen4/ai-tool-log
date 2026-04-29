#include "logfiltercontroller.h"
#include "logfilter.h"
#include "ui_mainwindow.h"

#include <QtConcurrent/QtConcurrent>
#include <QLineEdit>
#include <QRadioButton>

LogFilterController::LogFilterController(Ui::MainWindow *ui, QObject *parent)
    : QObject(parent)
    , m_ui(ui)
{}

FilterCriteria LogFilterController::buildCriteria() const
{
    FilterCriteria criteria;

    criteria.keywordFilter = m_ui->txtKeyword->text().trimmed();
    if (!criteria.keywordFilter.isEmpty())
        criteria.keywordRegex = QRegularExpression(criteria.keywordFilter,
                                                   QRegularExpression::CaseInsensitiveOption);

    FilterCriteria::applyFilter(criteria.messageFilter, criteria.messageOperator,
                                m_ui->txtFindMessage->text());
    FilterCriteria::applyFilter(criteria.tagFilter,     criteria.tagOperator,
                                m_ui->txtTagFilter->text());
    FilterCriteria::applyFilter(criteria.packageFilter, criteria.packageOperator,
                                m_ui->txtPackageFilter->text());
    FilterCriteria::applyFilter(criteria.pidFilter,     criteria.pidOperator,
                                m_ui->txtPidFilter->text());

    criteria.startTime   = m_ui->txtStartTime->text();
    criteria.endTime     = m_ui->txtEndTime->text();
    criteria.tidFilter   = QString();
    criteria.tidOperator = FilterOperator::OR;

    if      (m_ui->radioVerbosePlus->isChecked()) criteria.minLevel = "V";
    else if (m_ui->radioV->isChecked())           criteria.minLevel = "V";
    else if (m_ui->radioD->isChecked())           criteria.minLevel = "D";
    else if (m_ui->radioI->isChecked())           criteria.minLevel = "I";
    else if (m_ui->radioW->isChecked())           criteria.minLevel = "W";
    else if (m_ui->radioE->isChecked())           criteria.minLevel = "E";
    else if (m_ui->radioA->isChecked())           criteria.minLevel = "A";

    criteria.parsedMessage = ParsedFilter::build(criteria.messageFilter);
    criteria.parsedTag     = ParsedFilter::build(criteria.tagFilter);
    criteria.parsedPackage = ParsedFilter::build(criteria.packageFilter);
    criteria.parsedPid     = ParsedFilter::build(criteria.pidFilter);
    criteria.parsedTid     = ParsedFilter::build(criteria.tidFilter);

    criteria.minLevelIndex = LogFilter::levelIndex(criteria.minLevel);

    return criteria;
}

LogFilterController::Result LogFilterController::apply(const QVector<LogEntry> &allLogs,
                                                       const FilterCriteria &criteria) const
{
    Result r;
    r.filtered = QtConcurrent::blockingFiltered(allLogs,
        [&criteria, this](const LogEntry &entry) {
            return m_logFilter.passesFilter(entry, criteria);
        });

    r.index.reserve(r.filtered.size());
    for (int i = 0; i < r.filtered.size(); ++i)
        r.index[r.filtered[i].id] = i;

    return r;
}

bool LogFilterController::passesCurrent(const LogEntry &entry) const
{
    return m_logFilter.passesFilter(entry, buildCriteria());
}
