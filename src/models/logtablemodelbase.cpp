#include "logtablemodelbase.h"

#include "colorscheme.h"
#include "tableconfig.h"

#include <QFont>

namespace {
/** Bold font used for the level column; built once, not per painted cell. */
const QFont &levelFont()
{
    static const QFont f = []() {
        QFont font;
        font.setBold(true);
        return font;
    }();
    return f;
}
} // namespace

LogTableModelBase::LogTableModelBase(QObject *parent)
    : QAbstractTableModel(parent)
{
    // Level foregrounds and row tints come from ColorScheme, so a theme
    // switch must repaint every cell.
    connect(&ColorScheme::instance(), &ColorScheme::modeChanged, this, [this]() {
        refreshAllRows({Qt::ForegroundRole, Qt::BackgroundRole});
    });
}

QVariant LogTableModelBase::entryData(const LogEntry &entry, int column, int role) const
{
    using namespace TableConfig::LogColumns;

    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case DATE:    return entry.date;
        case TIME:    return entry.time;
        case PID:     return entry.pid;
        case TID:     return entry.tid;
        case PACKAGE: return entry.package;
        case LEVEL:   return entry.level;
        case TAG:     return entry.tag;
        case MESSAGE: return entry.message;
        default:      return {};
        }

    case Qt::TextAlignmentRole:
        return column == DATE ? QVariant(Qt::AlignCenter)
                              : QVariant(int(Qt::AlignLeft | Qt::AlignVCenter));

    case Qt::ForegroundRole:
        return ColorScheme::instance().levelColor(entry.level);

    case Qt::FontRole:
        return column == LEVEL ? QVariant(levelFont()) : QVariant();

    default:
        return {};
    }
}

QVariant LogTableModelBase::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};

    if (orientation == Qt::Vertical)
        return section + 1;

    using namespace TableConfig::LogColumns;
    switch (section) {
    case DATE:    return Names::DATE;
    case TIME:    return Names::TIME;
    case PID:     return Names::PID;
    case TID:     return Names::TID;
    case PACKAGE: return Names::PACKAGE;
    case LEVEL:   return Names::LEVEL;
    case TAG:     return Names::TAG;
    case MESSAGE: return Names::MESSAGE;
    case DELTA:   return Names::DELTA;
    default:      return {};
    }
}

void LogTableModelBase::refreshAllRows(const QList<int> &roles)
{
    const int rows = rowCount();
    const int cols = columnCount();
    if (rows <= 0 || cols <= 0)
        return;
    emit dataChanged(index(0, 0), index(rows - 1, cols - 1), roles);
}

void LogTableModelBase::refreshColumn(int column, const QList<int> &roles)
{
    const int rows = rowCount();
    if (rows <= 0 || column < 0 || column >= columnCount())
        return;
    emit dataChanged(index(0, column), index(rows - 1, column), roles);
}
