// UiManager: eventFilter dispatch plus the wheel / focus handlers it delegates to.
#include "uimanager.h"
#include "devicestabcontroller.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logmodel.h"
#include "logsplitcontroller.h"

#include <QCompleter>
#include <QEvent>
#include <QLineEdit>
#include <QScrollBar>
#include <QStyle>
#include <QTableView>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {
/** Object-name prefix of the per-device rows in the Devices tab right panel. */
constexpr auto kSelectedRowPrefix = "devSelectedRow_";
} // namespace

void UiManager::resizeVisibleRows()
{
    // Word-wrapped rows have to be measured to be sized, which costs a layout
    // pass per row. Only the rows actually on screen are measured; a buffer
    // of thousands would otherwise make every scroll tick quadratic.
    const auto resizeOne = [this](QTableView *view, LogModel *model) {
        if (!view || !model)
            return;
        QWidget *viewport = view->viewport();
        const int rowCount = model->rowCount();
        if (rowCount == 0 || !viewport)
            return;

        int first = view->rowAt(0);
        if (first < 0)
            first = 0;
        int last = view->rowAt(viewport->height() - 1);
        if (last < 0 || last >= rowCount)
            last = rowCount - 1;

        for (int row = first; row <= last; ++row)
            view->resizeRowToContents(row);

        // A pending scroll target may be off screen; size it too so the
        // scrollTo() below lands on an already-correct row height.
        if (m_pendingCenterRow >= 0 && m_pendingCenterRow < rowCount
            && (m_pendingCenterRow < first || m_pendingCenterRow > last))
            view->resizeRowToContents(m_pendingCenterRow);
    };

    resizeOne(m_ui->tableLog, m_logModel);
    if (m_logSplitController && m_logSplitController->isSplit()) {
        if (auto *paneB = m_logSplitController->paneB())
            resizeOne(paneB->table, paneB->model);
    }

    if (m_pendingCenterRow < 0)
        return;

    QTableView *target = activeTableLog();
    LogModel   *model  = activeLogModel();
    if (target && model && m_pendingCenterRow < model->rowCount())
        target->scrollTo(model->index(m_pendingCenterRow, 0),
                         QAbstractItemView::PositionAtCenter);
    m_pendingCenterRow = -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Log Navigation Helpers
// ─────────────────────────────────────────────────────────────────────────────

int UiManager::findLogInFilteredLogs(int allLogsIndex) const
{
    const QVector<LogEntry> &all = activeAllLogs();
    if (allLogsIndex < 0 || allLogsIndex >= all.size())
        return -1;
    return activeFilteredLogsIndex().value(all.at(allLogsIndex).id, -1);
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Event filter (delegated from MainWindow::eventFilter)
// ─────────────────────────────────────────────────────────────────────────────

bool UiManager::handleEvent(QObject *obj, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonRelease:
        handleDeviceRowClick(qobject_cast<QWidget *>(obj));
        return false;

    case QEvent::Wheel:
        return handleShiftScrollEvent(obj, static_cast<QWheelEvent *>(event));

    case QEvent::FocusIn:
        handleCompleterFocusEvent(obj, event);
        return false;

    default:
        return false;
    }
}

void UiManager::handleDeviceRowClick(QWidget *row)
{
    if (!row)
        return;

    // Sidebar row: select it directly.
    const auto it = m_deviceRowMap.constFind(row);
    if (it != m_deviceRowMap.constEnd()) {
        m_devicesTabController->selectDeviceRow(row, it.value());
        return;
    }

    // Right-panel row: its object name carries the device serial.
    const QString objectName = row->objectName();
    if (!objectName.startsWith(QLatin1String(kSelectedRowPrefix)))
        return;
    const QString serial = objectName.mid(int(qstrlen(kSelectedRowPrefix)));

    highlightSelectedDeviceRow(row);

    for (const AdbDevice &device : DevicesManager::instance().connectedDevices()) {
        if (device.id != serial)
            continue;

        DeviceInfo info;
        info.serial = device.id;
        info.name   = device.name;
        info.online = device.isOnline;
        m_devicesTabController->updateDeviceDetails(info);

        // Mirror the selection onto the matching sidebar row.
        for (auto entry = m_deviceRowMap.constBegin();
             entry != m_deviceRowMap.constEnd(); ++entry) {
            if (entry.value().serial == serial) {
                m_devicesTabController->selectDeviceRow(entry.key(), entry.value());
                break;
            }
        }
        return;
    }
}

void UiManager::highlightSelectedDeviceRow(QWidget *selected)
{
    QVBoxLayout *listLayout = m_ui->devDeviceListVLayout;
    if (!listLayout)
        return;

    // Selection is a state, not a stylesheet: flag it and let the theme sheet
    // paint it, so the rows follow a light/dark switch like everything else.
    for (int i = 0; i < listLayout->count(); ++i) {
        QWidget *row = listLayout->itemAt(i)->widget();
        if (!row)
            continue;
        row->setProperty("deviceRow", true);
        row->setProperty("selected", row == selected);
        row->style()->unpolish(row);
        row->style()->polish(row);
    }
}

bool UiManager::handleShiftScrollEvent(QObject *obj, QWheelEvent *wheelEvent)
{
    if (!(wheelEvent->modifiers() & Qt::ShiftModifier))
        return false;

    // Resolve the viewport back to its table. Pane B's tables are included so
    // shift-scroll behaves the same in a split view.
    QTableView *table = nullptr;
    const auto match = [obj, &table](QTableView *candidate) {
        if (candidate && obj == candidate->viewport())
            table = candidate;
    };
    match(m_ui->tableLog);
    match(m_ui->tableMarkLog);
    if (m_logSplitController) {
        if (auto *paneB = m_logSplitController->paneB()) {
            match(paneB->table);
            match(paneB->markTable);
        }
    }
    if (!table)
        return false;

    QScrollBar *horizontal = table->horizontalScrollBar();
    if (!horizontal)
        return false;

    const int steps = wheelEvent->angleDelta().y() / QWheelEvent::DefaultDeltasPerStep;
    horizontal->setValue(horizontal->value() - steps * horizontal->singleStep());
    return true;
}

bool UiManager::handleCompleterFocusEvent(QObject *obj, QEvent *event)
{
    if (obj != m_ui->txtPropertySearch || event->type() != QEvent::FocusIn)
        return false;
    if (!m_ui->txtPropertySearch->text().isEmpty())
        return false;

    QCompleter *completer = m_ui->txtPropertySearch->completer();
    if (completer && completer->model() && completer->model()->rowCount() > 0) {
        completer->setCompletionPrefix(QString());
        completer->complete();
    }
    return false;
}
