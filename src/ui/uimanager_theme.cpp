// UiManager: theme, log/output view font, column visibility and status-bar updates.
#include "uimanager.h"
#include "ui_mainwindow.h"
#include "adbmanager.h"
#include "colorscheme.h"
#include "tableconfig.h"
#include "tablestyler.h"
#include "themesheets.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QHeaderView>
#include <QSettings>
#include <QStatusBar>
#include <QTableView>

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

/** QSettings key of the log/output view font; absent means the theme default. */
constexpr auto kViewFontKey = "Appearance/viewFont";
/** Log-table text size in the theme sheet, used while no view font is chosen. */
constexpr int kDefaultViewFontPixelSize = 12;
/** Cell padding plus the row divider, on top of the text height of a log row. */
constexpr int kLogRowChromeHeight = 10;
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

    // Hide the time-range filter while the Time column is off — filtering on a
    // column you cannot see is only confusing.
    if (vis.size() > TIME)
        m_ui->groupBox_6->setVisible(vis[TIME]);
}

void UiManager::applyCurrentTheme()
{
    const bool light = ColorScheme::instance().resolvedMode() == ColorScheme::Mode::Light;
    QString sheet = light ? ThemeSheets::lightStylesheet() : m_darkStylesheet;
    // The view font rides in the same sheet: set on the widgets directly, it
    // would lose to the theme's own font rules for those views.
    if (m_viewFont)
        sheet += ThemeSheets::viewFontRules(*m_viewFont, QApplication::font().family());
    if (qApp)
        qApp->setStyleSheet(sheet);
}

std::optional<QFont> UiManager::savedViewFont()
{
    const QString stored = QSettings().value(QLatin1String(kViewFontKey)).toString();
    QFont font;
    if (stored.isEmpty() || !font.fromString(stored))
        return std::nullopt;
    return font;
}

void UiManager::applyViewFont(const std::optional<QFont> &font)
{
    // Only the log tables and output views take this font; buttons, inputs
    // and the rest of the interface keep the application font.
    m_viewFont = font;

    QSettings settings;
    if (font)
        settings.setValue(QLatin1String(kViewFontKey), font->toString());
    else
        settings.remove(QLatin1String(kViewFontKey));

    applyCurrentTheme();

    fitLogRowHeight(m_ui->tableLog);
    fitLogRowHeight(m_ui->tableMarkLog);
    if (auto *paneB = m_logSplitController ? m_logSplitController->paneB() : nullptr) {
        fitLogRowHeight(paneB->table);
        fitLogRowHeight(paneB->markTable);
    }
    if (m_rowResizeTimer)
        m_rowResizeTimer->start();   // word-wrapped rows have to be measured again
}

QFont UiManager::defaultViewFont() const
{
    // Mirrors the theme sheet's log-table rule: the application family at 12px.
    QFont font = QApplication::font();
    font.setPixelSize(kDefaultViewFontPixelSize);
    return font;
}

void UiManager::fitLogRowHeight(QTableView *view)
{
    if (!view || !view->verticalHeader())
        return;
    // Measure the font the stylesheet assigns rather than view->font(): a
    // table built a moment ago has not been polished yet.
    const QFontMetrics metrics(m_viewFont ? *m_viewFont : defaultViewFont());
    view->verticalHeader()->setDefaultSectionSize(
        qMax(TableStyler::Metrics::kLogRowHeight, metrics.height() + kLogRowChromeHeight));
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
