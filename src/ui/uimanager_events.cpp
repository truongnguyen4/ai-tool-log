// UiManager: eventFilter + global key/wheel/focus handlers.
// split out of uimanager.cpp.
#include "uimanager.h"
#include "devicestabcontroller.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "colorscheme.h"
#include "logmodel.h"
#include "marklogmodel.h"

#include <QApplication>
#include <QCompleter>
#include <QEvent>
#include <QFocusEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QScrollBar>
#include <QTableView>
#include <QWheelEvent>

void UiManager::resizeVisibleRows()
{
    auto resizeOne = [this](QTableView *view, LogModel *model) {
        if (!view || !model) return;
        auto *vp = view->viewport();
        const int rowCount = model->rowCount();
        if (rowCount == 0 || !vp) return;

        int first = view->rowAt(0);
        if (first < 0) first = 0;
        int last  = view->rowAt(vp->height() - 1);
        if (last < 0 || last >= rowCount) last = rowCount - 1;

        for (int r = first; r <= last; ++r)
            view->resizeRowToContents(r);

        if (m_pendingCenterRow >= 0 &&
            (m_pendingCenterRow < first || m_pendingCenterRow > last))
            view->resizeRowToContents(m_pendingCenterRow);
    };

    // Pane A
    resizeOne(m_ui->tableLog, m_logModel);

    // Pane B (when split is active)
    if (m_logSplitController && m_logSplitController->isSplit()) {
        if (auto *pb = m_logSplitController->paneB())
            resizeOne(pb->table, pb->model);
    }

    if (m_pendingCenterRow >= 0) {
        QTableView *target = activeTableLog();
        LogModel   *tgtMod = activeLogModel();
        if (target && tgtMod)
            target->scrollTo(tgtMod->index(m_pendingCenterRow, 0),
                             QAbstractItemView::PositionAtCenter);
        m_pendingCenterRow = -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Log Navigation Helpers
// ─────────────────────────────────────────────────────────────────────────────

int UiManager::findLogInAllLogs(const LogEntry &entry) const
{
    return activeAllLogsIndex().value(entry.id, -1);
}

int UiManager::findLogInFilteredLogs(int allLogsIndex) const
{
    const auto &all = activeAllLogs();
    if (allLogsIndex < 0 || allLogsIndex >= all.size()) return -1;
    return activeFilteredLogsIndex().value(all[allLogsIndex].id, -1);
}

int UiManager::findNearestVisibleLog(int allLogsIndex) const
{
    const auto &all     = activeAllLogs();
    const auto &filtIdx = activeFilteredLogsIndex();
    if (allLogsIndex < 0 || allLogsIndex >= all.size()) return -1;
    for (int i = allLogsIndex + 1; i < all.size(); ++i) {
        const int row = filtIdx.value(all[i].id, -1);
        if (row >= 0) return row;
    }
    for (int i = allLogsIndex - 1; i >= 0; --i) {
        const int row = filtIdx.value(all[i].id, -1);
        if (row >= 0) return row;
    }
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Event Filter Helpers (delegated from MainWindow::eventFilter)
// ─────────────────────────────────────────────────────────────────────────────

bool UiManager::handleEvent(QObject *obj, QEvent *event)
{
    QWidget *w = qobject_cast<QWidget*>(obj);

    // ── Device row: click to select ───────────────────────────────────────────
    if (event->type() == QEvent::MouseButtonRelease) {
        if (w && m_deviceRowMap.contains(w)) {
            m_devicesTabController->selectDeviceRow(w, m_deviceRowMap[w]);
            return false;
        }
        // ── Selected device row in right panel: click to show details ─────────
        if (w && w->objectName().startsWith(QStringLiteral("devSelectedRow_"))) {
            const QString serial = w->objectName().mid(
                static_cast<int>(QString("devSelectedRow_").length()));

            // Highlight the clicked row, reset others
            const ColorScheme &cs = ColorScheme::instance();
            const QString colBorder = ColorScheme::toHex(cs.border());
            const QString colRowSel = ColorScheme::toHex(cs.rowSelectedBackground());
            QVBoxLayout *listLayout = m_ui->devDeviceListVLayout;
            for (int i = 0; i < listLayout->count(); ++i) {
                if (QWidget *row = listLayout->itemAt(i)->widget()) {
                    if (row == w)
                        row->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid %2;")
                                               .arg(colRowSel, colBorder));
                    else
                        row->setStyleSheet(QString("background: transparent; border-bottom: 1px solid %1;")
                                               .arg(colBorder));
                }
            }

            // Find the device in connected list and show its details
            DevicesManager &dm = DevicesManager::instance();
            const QList<AdbDevice> connected = dm.connectedDevices();
            for (const AdbDevice &d : connected) {
                if (d.id == serial) {
                    DeviceInfo info;
                    info.serial = d.id;
                    info.name   = d.name;
                    info.online = d.isOnline;
                    m_devicesTabController->updateDeviceDetails(info);
                    // Also highlight corresponding sidebar row
                    for (auto it = m_deviceRowMap.constBegin(); it != m_deviceRowMap.constEnd(); ++it) {
                        if (it.value().serial == serial) {
                            m_devicesTabController->selectDeviceRow(it.key(), it.value());
                            break;
                        }
                    }
                    break;
                }
            }
            return false;
        }
    }

    if (event->type() == QEvent::Wheel)
        if (handleShiftScrollEvent(obj, static_cast<QWheelEvent*>(event))) return true;
    handleCompleterFocusEvent(obj, event);
    return false;
}

bool UiManager::handleShiftScrollEvent(QObject *obj, QWheelEvent *wheelEvent)
{
    if (!(wheelEvent->modifiers() & Qt::ShiftModifier)) return false;

    QTableView *tableView = nullptr;
    if      (obj == m_ui->tableLog->viewport())     tableView = m_ui->tableLog;
    else if (obj == m_ui->tableMarkLog->viewport()) tableView = m_ui->tableMarkLog;
    if (!tableView) return false;

    QScrollBar *hBar = tableView->horizontalScrollBar();
    if (!hBar) return false;

    const int steps = wheelEvent->angleDelta().y() / 120;
    hBar->setValue(hBar->value() - steps * hBar->singleStep());
    return true;
}

bool UiManager::handleCompleterFocusEvent(QObject *obj, QEvent *event)
{
    if (obj == m_ui->txtPropertySearch && event->type() == QEvent::FocusIn) {
        if (m_ui->txtPropertySearch->text().isEmpty()) {
            QCompleter *c = m_ui->txtPropertySearch->completer();
            if (c && c->model() && c->model()->rowCount() > 0) {
                c->setCompletionPrefix("");
                c->complete();
            }
        }
    }
    return false;
}

