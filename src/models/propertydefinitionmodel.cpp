#include "propertydefinitionmodel.h"
#include "blinksweep.h"
#include "colorscheme.h"
#include "tableconfig.h"

#include <QBrush>
#include <QFont>
#include <QSet>
#include <QTimer>

#include <algorithm>

namespace {

using namespace TableConfig::PropertyDefColumns;

/** Opacity of the row tint behind a staged change or a failed write. */
constexpr int kStagedTintAlpha = 40;

bool sameNames(const QVector<PropertyDefinition> &a, const QVector<PropertyDefinition> &b)
{
    if (a.size() != b.size())
        return false;
    for (qsizetype i = 0; i < a.size(); ++i) {
        if (a.at(i).name != b.at(i).name)
            return false;
    }
    return true;
}

QColor tinted(QColor color)
{
    color.setAlpha(kStagedTintAlpha);
    return color;
}

} // namespace

PropertyDefinitionModel::PropertyDefinitionModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_clock.start();
    m_blinkSweep = new QTimer(this);
    BlinkSweep::installForModel(m_blinkSweep, &m_blinkUntil, &m_clock, this);
}

void PropertyDefinitionModel::scheduleBlinkSweep()
{
    if (!m_blinkSweep->isActive())
        m_blinkSweep->start();
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: QAbstractTableModel
// ─────────────────────────────────────────────────────────────────────────────

int PropertyDefinitionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int PropertyDefinitionModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : TOTAL_COLUMNS;
}

QVariant PropertyDefinitionModel::data(const QModelIndex &index, int role) const
{
    const PropertyDefinition *defPtr = index.isValid() ? definitionAt(index.row()) : nullptr;
    if (!defPtr)
        return QVariant();
    const PropertyDefinition &def = *defPtr;
    const auto pending = m_pending.constFind(def.name);
    const bool staged = pending != m_pending.constEnd();
    const int column = index.column();

    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case NAME:    return def.name;
        case VALUE:
            if (!def.available)
                return tr("n/a");
            // A checkbox already says true or false.
            return def.kind() == PropertyDefinition::Kind::Boolean ? QVariant() : shownValue(def);
        case DEFAULT: return def.defaultValue;
        case TYPE:    return def.typeSummary();
        case ID:      return def.id;
        case NOTES:   return notes(def);
        }
        break;

    case Qt::EditRole:
        return column == VALUE ? shownValue(def) : data(index, Qt::DisplayRole);

    case Qt::CheckStateRole:
        if (column == VALUE && def.available && def.kind() == PropertyDefinition::Kind::Boolean) {
            return def.sameValue(shownValue(def), QStringLiteral("true")) ? Qt::Checked
                                                                            : Qt::Unchecked;
        }
        break;

    case Qt::ToolTipRole:
        return tooltip(def);

    case Qt::ForegroundRole: {
        const ColorScheme &colors = ColorScheme::instance();
        if (staged && !pending->error.isEmpty() && (column == VALUE || column == NAME))
            return QBrush(colors.danger());
        if (staged && (column == VALUE || column == NAME))
            return QBrush(colors.accent());
        if (!def.available || (def.readOnly && column != NAME))
            return QBrush(colors.mutedText());
        if (column == DEFAULT || column == TYPE || column == ID || column == NOTES)
            return QBrush(colors.mutedText());
        break;
    }

    case Qt::FontRole:
        // Bold marks what differs from the default; a staged change is also
        // italic, so it reads as "not on the device yet".
        if (column == VALUE && (staged || def.isModified())) {
            QFont font;
            font.setBold(true);
            font.setItalic(staged);
            return font;
        }
        break;

    case Qt::BackgroundRole: {
        const auto blink = m_blinkUntil.constFind(def.name);
        if (blink != m_blinkUntil.constEnd() && blink.value() > m_clock.elapsed())
            return QBrush(ColorScheme::instance().blinkBackground());
        if (staged) {
            const ColorScheme &colors = ColorScheme::instance();
            return QBrush(tinted(pending->error.isEmpty() ? colors.accent() : colors.danger()));
        }
        break;
    }

    case Qt::TextAlignmentRole:
        if (column == ID)
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        break;
    }
    return QVariant();
}

QVariant PropertyDefinitionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    switch (section) {
    case NAME:    return tr(Names::NAME);
    case VALUE:   return tr(Names::VALUE);
    case DEFAULT: return tr(Names::DEFAULT);
    case TYPE:    return tr(Names::TYPE);
    case ID:      return tr(Names::ID);
    case NOTES:   return tr(Names::NOTES);
    }
    return QVariant();
}

Qt::ItemFlags PropertyDefinitionModel::flags(const QModelIndex &index) const
{
    const PropertyDefinition *def = index.isValid() ? definitionAt(index.row()) : nullptr;
    if (!def)
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == VALUE && def->isWritable()) {
        flags |= def->kind() == PropertyDefinition::Kind::Boolean ? Qt::ItemIsUserCheckable
                                                                  : Qt::ItemIsEditable;
    }
    return flags;
}

bool PropertyDefinitionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    const PropertyDefinition *def = index.isValid() ? definitionAt(index.row()) : nullptr;
    if (!def || index.column() != VALUE)
        return false;

    QString text;
    if (role == Qt::CheckStateRole)
        text = Qt::CheckState(value.toInt()) == Qt::Checked ? QStringLiteral("true")
                                                            : QStringLiteral("false");
    else if (role == Qt::EditRole)
        text = value.toString();
    else
        return false;

    // Refused values are reported by the delegate or caller that validated
    // them first; here they are simply not taken.
    return stage(def->name, text).isEmpty();
}

void PropertyDefinitionModel::sort(int column, Qt::SortOrder order)
{
    if (column == m_sortColumn && order == m_sortOrder)
        return;
    m_sortColumn = column;
    m_sortOrder  = order;

    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    const QModelIndexList before = persistentIndexList();
    QVector<int> catalogIndexes;
    catalogIndexes.reserve(before.size());
    for (const QModelIndex &idx : before)
        catalogIndexes.append(m_rows.value(idx.row(), -1));

    sortIndexes(m_rows);
    rebuildRowLookup();

    QModelIndexList after;
    after.reserve(before.size());
    for (int i = 0; i < before.size(); ++i) {
        const int row = m_rowByIndex.value(catalogIndexes.at(i), -1);
        after.append(row < 0 ? QModelIndex() : index(row, before.at(i).column()));
    }
    changePersistentIndexList(before, after);
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Catalog
// ─────────────────────────────────────────────────────────────────────────────

void PropertyDefinitionModel::setDefinitions(const QVector<PropertyDefinition> &definitions)
{
    const bool refresh = !m_definitions.isEmpty() && sameNames(m_definitions, definitions);

    if (refresh) {
        const qint64 deadline = m_clock.elapsed() + BlinkSweep::kBlinkDurationMs;
        for (qsizetype i = 0; i < definitions.size(); ++i) {
            if (m_definitions.at(i).value != definitions.at(i).value)
                m_blinkUntil.insert(definitions.at(i).name, deadline);
        }
        if (!m_blinkUntil.isEmpty())
            scheduleBlinkSweep();
    } else {
        m_blinkUntil.clear();
    }

    m_definitions = definitions;
    m_indexByName.clear();
    m_indexById.clear();
    for (int i = 0; i < m_definitions.size(); ++i) {
        m_indexByName.insert(m_definitions.at(i).name, i);
        m_indexById.insert(m_definitions.at(i).id, i);
    }

    // A staged change the device now holds is done; one for a property that
    // is gone or no longer writable can never be applied.
    bool pendingDropped = false;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        const PropertyDefinition *def = find(it.key());
        if (!def || !def->isWritable() || def->sameValue(def->value, it->value)) {
            it = m_pending.erase(it);
            pendingDropped = true;
        } else {
            ++it;
        }
    }

    refreshRows(!refresh);
    if (pendingDropped)
        emit pendingChanged(pendingCount());
}

void PropertyDefinitionModel::clear()
{
    const bool hadPending = !m_pending.isEmpty();
    beginResetModel();
    m_definitions.clear();
    m_indexByName.clear();
    m_indexById.clear();
    m_rows.clear();
    m_rowByIndex.clear();
    m_pending.clear();
    m_blinkUntil.clear();
    endResetModel();
    if (hadPending)
        emit pendingChanged(0);
}

const PropertyDefinition *PropertyDefinitionModel::definitionAt(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return nullptr;
    return &m_definitions.at(m_rows.at(row));
}

const PropertyDefinition *PropertyDefinitionModel::find(const QString &name) const
{
    const auto it = m_indexByName.constFind(name);
    return it == m_indexByName.constEnd() ? nullptr : &m_definitions.at(it.value());
}

const PropertyDefinition *PropertyDefinitionModel::findById(int id) const
{
    const auto it = m_indexById.constFind(id);
    return it == m_indexById.constEnd() ? nullptr : &m_definitions.at(it.value());
}

int PropertyDefinitionModel::rowOf(const QString &name) const
{
    const auto it = m_indexByName.constFind(name);
    return it == m_indexByName.constEnd() ? -1 : m_rowByIndex.value(it.value(), -1);
}

int PropertyDefinitionModel::modifiedCount() const
{
    return int(std::count_if(m_definitions.cbegin(), m_definitions.cend(),
                             [](const PropertyDefinition &d) { return d.isModified(); }));
}

int PropertyDefinitionModel::unavailableCount() const
{
    return int(std::count_if(m_definitions.cbegin(), m_definitions.cend(),
                             [](const PropertyDefinition &d) { return !d.available; }));
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Filter, scope and sort
// ─────────────────────────────────────────────────────────────────────────────

void PropertyDefinitionModel::setFilter(const QString &text)
{
    m_query = PropertyDefinitionQuery::parse(text);
    refreshRows(true);
}

void PropertyDefinitionModel::setScope(Scope scope)
{
    if (scope == m_scope)
        return;
    m_scope = scope;
    refreshRows(true);
}

bool PropertyDefinitionModel::passes(const PropertyDefinition &def) const
{
    const bool staged = m_pending.contains(def.name);
    switch (m_scope) {
    case Scope::All:         break;
    case Scope::Modified:    if (!def.isModified() && !staged) return false; break;
    case Scope::Pending:     if (!staged) return false; break;
    case Scope::Writable:    if (!def.isWritable()) return false; break;
    case Scope::ReadOnly:    if (!def.readOnly) return false; break;
    case Scope::NeedsReboot: if (!def.needReboot) return false; break;
    case Scope::Unavailable: if (def.available) return false; break;
    }
    return m_query.matches(def, shownValue(def));
}

QVector<int> PropertyDefinitionModel::visibleIndexes() const
{
    QVector<int> indexes;
    indexes.reserve(m_definitions.size());
    for (int i = 0; i < m_definitions.size(); ++i) {
        if (passes(m_definitions.at(i)))
            indexes.append(i);
    }
    sortIndexes(indexes);
    return indexes;
}

void PropertyDefinitionModel::sortIndexes(QVector<int> &indexes) const
{
    if (m_sortColumn < 0)
        return;

    const auto key = [this](const PropertyDefinition &def) -> QString {
        switch (m_sortColumn) {
        case VALUE:   return shownValue(def);
        case DEFAULT: return def.defaultValue;
        case TYPE:    return def.type;
        case NOTES:   return notes(def);
        default:      return def.name;
        }
    };
    const bool descending = m_sortOrder == Qt::DescendingOrder;
    std::stable_sort(indexes.begin(), indexes.end(), [&](int a, int b) {
        const PropertyDefinition &x = m_definitions.at(a);
        const PropertyDefinition &y = m_definitions.at(b);
        int order = 0;
        if (m_sortColumn == ID) {
            order = x.id < y.id ? -1 : (x.id > y.id ? 1 : 0);
        } else {
            // Numbers sort by value, so 10 comes after 9.
            const QString kx = key(x);
            const QString ky = key(y);
            bool okX = false, okY = false;
            const qint64 nx = kx.toLongLong(&okX);
            const qint64 ny = ky.toLongLong(&okY);
            order = okX && okY ? (nx < ny ? -1 : (nx > ny ? 1 : 0))
                               : kx.compare(ky, Qt::CaseInsensitive);
        }
        if (order == 0)
            return x.name < y.name;   // ties keep a stable, readable order
        return descending ? order > 0 : order < 0;
    });
}

void PropertyDefinitionModel::refreshRows(bool forceReset)
{
    QVector<int> rows = visibleIndexes();
    if (!forceReset && rows == m_rows) {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0, 0), index(rowCount() - 1, TOTAL_COLUMNS - 1));
        return;
    }
    beginResetModel();
    m_rows = std::move(rows);
    rebuildRowLookup();
    endResetModel();
}

void PropertyDefinitionModel::rebuildRowLookup()
{
    m_rowByIndex.clear();
    m_rowByIndex.reserve(m_rows.size());
    for (int row = 0; row < m_rows.size(); ++row)
        m_rowByIndex.insert(m_rows.at(row), row);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Staged changes
// ─────────────────────────────────────────────────────────────────────────────

QString PropertyDefinitionModel::shownValue(const PropertyDefinition &def) const
{
    const auto it = m_pending.constFind(def.name);
    return it == m_pending.constEnd() ? def.value : it->value;
}

QString PropertyDefinitionModel::stage(const QString &name, const QString &value)
{
    const PropertyDefinition *def = find(name);
    if (!def)
        return tr("%1 is not a property of this device.").arg(name);

    QString normalized;
    const QString error = def->validate(value, &normalized);
    if (!error.isEmpty())
        return error;

    if (def->sameValue(def->value, normalized)) {
        if (m_pending.remove(name) > 0) {
            emitRowChanged(name);
            emit pendingChanged(pendingCount());
        }
        return QString();
    }

    const auto existing = m_pending.constFind(name);
    if (existing != m_pending.constEnd() && existing->value == normalized && existing->error.isEmpty())
        return QString();
    m_pending.insert(name, Pending{normalized, QString()});
    emitRowChanged(name);
    emit pendingChanged(pendingCount());
    return QString();
}

void PropertyDefinitionModel::unstage(const QStringList &names)
{
    bool changed = false;
    for (const QString &name : names) {
        if (m_pending.remove(name) > 0) {
            emitRowChanged(name);
            changed = true;
        }
    }
    if (changed)
        emit pendingChanged(pendingCount());
}

void PropertyDefinitionModel::discardAll()
{
    if (m_pending.isEmpty())
        return;
    const QStringList names = m_pending.keys();
    m_pending.clear();
    for (const QString &name : names)
        emitRowChanged(name);
    emit pendingChanged(0);
}

void PropertyDefinitionModel::markApplied(const QVector<QPair<QString, QString>> &written)
{
    bool pendingDone = false;
    const qint64 deadline = m_clock.elapsed() + BlinkSweep::kBlinkDurationMs;
    for (const auto &[name, value] : written) {
        const auto index = m_indexByName.constFind(name);
        if (index == m_indexByName.constEnd())
            continue;
        PropertyDefinition &def = m_definitions[index.value()];
        def.value = value;
        const auto pending = m_pending.constFind(name);
        if (pending != m_pending.constEnd() && def.sameValue(pending->value, value)) {
            m_pending.erase(pending);
            pendingDone = true;
        }
        m_blinkUntil.insert(name, deadline);
        emitRowChanged(name);
    }
    if (!written.isEmpty())
        scheduleBlinkSweep();
    if (pendingDone)
        emit pendingChanged(pendingCount());
}

void PropertyDefinitionModel::setWriteError(const QString &name, const QString &error)
{
    const auto it = m_pending.find(name);
    if (it == m_pending.end())
        return;
    it->error = error;
    emitRowChanged(name);
}

QVector<QPair<QString, QString>> PropertyDefinitionModel::pendingChanges(bool includeFailed) const
{
    QVector<QPair<QString, QString>> changes;
    changes.reserve(m_pending.size());
    for (const PropertyDefinition &def : m_definitions) {
        const auto it = m_pending.constFind(def.name);
        if (it != m_pending.constEnd() && (includeFailed || it->error.isEmpty()))
            changes.append({def.name, it->value});
    }
    return changes;
}

void PropertyDefinitionModel::emitRowChanged(const QString &name)
{
    const int row = rowOf(name);
    if (row >= 0)
        emit dataChanged(index(row, 0), index(row, TOTAL_COLUMNS - 1));
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Presentation
// ─────────────────────────────────────────────────────────────────────────────

QString PropertyDefinitionModel::notes(const PropertyDefinition &def) const
{
    QStringList parts;
    if (!def.available)
        parts << tr("unavailable");
    if (def.readOnly)
        parts << tr("read-only");
    if (def.needReboot)
        parts << tr("reboot");
    return parts.join(QStringLiteral(" · "));
}

QString PropertyDefinitionModel::tooltip(const PropertyDefinition &def) const
{
    QStringList lines;
    lines << QStringLiteral("%1   (id %2)").arg(def.name).arg(def.id);
    lines << tr("Type: %1").arg(def.type);
    if (!def.typeRestriction.isEmpty())
        lines << tr("Class: %1").arg(def.typeRestriction);
    if (!def.allowedValues.isEmpty()) {
        QStringList values;
        for (int i = 0; i < def.allowedValues.size() && i < 40; ++i)
            values << QString::number(def.allowedValues.at(i));
        if (def.allowedValues.size() > 40)
            values << QStringLiteral("…");
        lines << tr("Allowed: %1").arg(values.join(QStringLiteral(", ")));
    } else if (def.hasRange) {
        lines << tr("Range: %1 – %2").arg(def.minimum).arg(def.maximum);
    }
    lines << tr("Default: %1").arg(def.defaultValue.isEmpty() ? tr("(empty)") : def.defaultValue);
    if (def.available)
        lines << tr("Device value: %1").arg(def.value.isEmpty() ? tr("(empty)") : def.value);

    const auto pending = m_pending.constFind(def.name);
    if (pending != m_pending.constEnd()) {
        lines << tr("Staged value: %1   — not written yet")
                     .arg(pending->value.isEmpty() ? tr("(empty)") : pending->value);
        if (!pending->error.isEmpty())
            lines << tr("Last write failed: %1").arg(pending->error);
    }

    QStringList flags;
    if (!def.available)
        flags << tr("Not available on this device");
    if (def.readOnly)
        flags << tr("Read-only");
    if (def.needReboot)
        flags << tr("Takes effect after a reboot");
    if (!flags.isEmpty())
        lines << flags.join(QStringLiteral(" · "));
    return lines.join(QLatin1Char('\n'));
}
