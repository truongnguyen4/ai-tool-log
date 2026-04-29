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

// Owns the LogFilter and the criteria-building logic that used to live in
// UiManager. Reads filter widget state from Ui::MainWindow and runs filtering
// over a log vector (uses QtConcurrent for parallelism). Stateless w.r.t.
// log data: callers pass in the source vector and receive the filtered
// vector + an id->row index map.
//
// For the actual matching primitives (logic / regex) used by every table in
// the app, see FilterEngine in src/filters/filterengine.h.
class LogFilterController : public QObject
{
    Q_OBJECT
public:
    struct Result {
        QVector<LogEntry>    filtered;
        QHash<quint64, int>  index;   // LogEntry::id -> row in `filtered`
    };

    explicit LogFilterController(Ui::MainWindow *ui, QObject *parent = nullptr);

    // Read the current state of the filter widgets and produce a FilterCriteria.
    FilterCriteria buildCriteria() const;

    // Apply the criteria to the full log set. Uses QtConcurrent for parallelism.
    Result apply(const QVector<LogEntry> &allLogs, const FilterCriteria &criteria) const;

    // One-off convenience: does this entry pass the current UI criteria?
    bool passesCurrent(const LogEntry &entry) const;

    const ILogFilter &logFilter() const { return m_logFilter; }

private:
    Ui::MainWindow *m_ui;
    LogFilter       m_logFilter;
};

#endif // LOGFILTERCONTROLLER_H
