// UiManager: theme, application font, column visibility and status-bar updates.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "colorscheme.h"
#include "tableconfig.h"
#include "themesheets.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QStatusBar>

#if defined(Q_OS_WIN)
#  include <windows.h>
#  include <psapi.h>
#elif defined(Q_OS_MACOS)
#  include <mach/mach.h>
#endif

namespace {
/** Minimum gap between resident-memory samples, in milliseconds. */
constexpr qint64 kMemorySampleIntervalMs = 1000;
constexpr qint64 kBytesPerMiB = 1024 * 1024;
} // namespace

void UiManager::applyColumnVisibility(const QVector<bool> &vis)
{
    using namespace TableConfig::LogColumns;
    auto *paneB = m_logSplitController ? m_logSplitController->paneB() : nullptr;

    for (int column = 0; column < vis.size(); ++column) {
        const bool hidden = !vis[column];
        m_ui->tableLog->setColumnHidden(column, hidden);
        m_ui->tableMarkLog->setColumnHidden(column, hidden);
        if (paneB && paneB->table)     paneB->table->setColumnHidden(column, hidden);
        if (paneB && paneB->markTable) paneB->markTable->setColumnHidden(column, hidden);
    }

    // Hide the filter group box for any column the user turned off — filtering
    // on a column you cannot see is only confusing.
    const auto setGroupVisible = [&vis](QWidget *group, int column) {
        if (group && vis.size() > column)
            group->setVisible(vis[column]);
    };
    setGroupVisible(m_ui->groupBox_2, PACKAGE);
    setGroupVisible(m_ui->groupBox_3, PID);
    setGroupVisible(m_ui->groupBox_6, TIME);
    setGroupVisible(m_ui->groupBox,   TAG);
    setGroupVisible(m_ui->groupBox_5, MESSAGE);
}

void UiManager::applyCurrentTheme()
{
    const bool light = ColorScheme::instance().resolvedMode() == ColorScheme::Mode::Light;
    if (qApp)
        qApp->setStyleSheet(light ? ThemeSheets::lightStylesheet() : m_darkStylesheet);
}

void UiManager::applyAppFont(const QFont &font)
{
    // Single source of truth: every widget (including the dynamically-created
    // pane B tables) inherits its font from the application via this loop.
    // No table-specific setFont calls live elsewhere — keep it that way.
    QApplication::setFont(font);
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        widget->setFont(font);
        widget->setStyleSheet(widget->styleSheet());   // force a re-polish
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SECTION: Status Bar & Performance
// ─────────────────────────────────────────────────────────────────────────────

void UiManager::updateMemoryUsage()
{
    // Sampling hits the OS, and updateStatusBar() runs on every filter pass and
    // log flush, so throttle to roughly one sample per second.
    if (m_memorySampleClock.isValid()
        && m_memorySampleClock.elapsed() < kMemorySampleIntervalMs)
        return;
    m_memorySampleClock.start();

#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        m_memoryUsageMb = static_cast<qint64>(counters.WorkingSetSize) / kBytesPerMiB;
#elif defined(Q_OS_MACOS)
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        m_memoryUsageMb = static_cast<qint64>(info.resident_size) / kBytesPerMiB;
#else
    // Linux and other /proc-based systems: VmRSS is reported in kB.
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    while (!status.atEnd()) {
        const QByteArray line = status.readLine();
        if (!line.startsWith("VmRSS:"))
            continue;
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() >= 2)
            m_memoryUsageMb = fields.at(1).toLongLong() / 1024;
        break;
    }
#endif
}

void UiManager::updateStatusBar()
{
    updateMemoryUsage();

    const AdbManager &adb = AdbManager::instance();
    const bool capturing = adb.isLogcatRunning() || adb.isDmesgRunning();

    m_ui->statusbar->showMessage(
        tr("UTF-8   Lines: %1   Mem: %2 MB   %3 %4")
            .arg(activeFilteredLogs().size())
            .arg(m_memoryUsageMb)
            .arg(capturing ? QStringLiteral("●") : QStringLiteral("○"),
                 capturing ? tr("Capturing") : tr("Idle")));
}
