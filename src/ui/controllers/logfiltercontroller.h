#ifndef LOGFILTERCONTROLLER_H
#define LOGFILTERCONTROLLER_H

#include <QObject>
#include <QVector>
#include <QHash>

#include "ilogfilter.h"
#include "logfilter.h"
#include "logentry.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ---------------------------------------------------------------------------
// LogFilterController — turns the filter widgets into a FilterCriteria and
// runs it over a log vector (in parallel, via QtConcurrent).
//
// Stateless with respect to log data: callers pass in the source vector and
// receive the filtered vector plus an id -> row index map.
//
// The criteria built from the widgets is cached. Building it parses every
// filter expression and compiles a regex, which is cheap once per filter
// change but ruinous once per log line — and passesCurrent() is called per
// line. Callers refresh the cache with refreshCriteria() whenever the filter
// widgets change.
//
// For the matching primitives shared with every other table in the app, see
// FilterEngine in src/filters/filterengine.h.
// ---------------------------------------------------------------------------
class LogFilterController : public QObject
{
    Q_OBJECT
public:
    struct Result {
        QVector<LogEntry>    filtered;
        QHash<quint64, int>  index;   // LogEntry::id -> row in `filtered`
    };

    explicit LogFilterController(Ui::MainWindow *ui, QObject *parent = nullptr);

    /** Read the filter widgets and produce a fresh FilterCriteria. */
    FilterCriteria buildCriteria() const;

    /** Rebuild and cache the criteria; returns the new value. */
    const FilterCriteria &refreshCriteria();

    /** The cached criteria, rebuilt on first use. */
    const FilterCriteria &criteria() const;

    /** Apply @p criteria to @p allLogs. Uses QtConcurrent for parallelism. */
    Result apply(const QVector<LogEntry> &allLogs, const FilterCriteria &criteria) const;

    /** Convenience: apply the cached criteria. */
    Result apply(const QVector<LogEntry> &allLogs) const { return apply(allLogs, criteria()); }

    /** Does this entry pass the cached criteria? */
    bool passesCurrent(const LogEntry &entry) const;

    const ILogFilter &logFilter() const { return m_logFilter; }

private:
    Ui::MainWindow          *m_ui;
    LogFilter                m_logFilter;
    mutable FilterCriteria   m_criteria;
    mutable bool             m_criteriaValid = false;
};

#endif // LOGFILTERCONTROLLER_H
