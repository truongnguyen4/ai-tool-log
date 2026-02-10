#ifndef TOOLTIPS_H
#define TOOLTIPS_H

// ---------------------------------------------------------------------------
// Central tooltip registry for ToolLogPro
// Add / update all button and widget tooltips here.
// Applied at runtime by MainWindow::setupTooltips().
// ---------------------------------------------------------------------------

namespace Tooltips {

// Top bar
inline constexpr const char *btnToggleTerminal   = "Toggle embedded terminal (Ctrl+`)";
inline constexpr const char *btnAppSettings      = "Open application settings";

// ADB Logcat toolbar
inline constexpr const char *btnStart            = "Start / stop ADB logcat capture";
inline constexpr const char *btnKernel           = "Start / stop kernel log stream (sudo dmesg -w)";
inline constexpr const char *btnAutoScroll       = "Auto-scroll to the latest log entry";
inline constexpr const char *btnColumns          = "Choose which columns to show";
inline constexpr const char *btnToggleCellContent = "Show / hide the cell content panel";
inline constexpr const char *btnClear            = "Clear all logs from the table";
inline constexpr const char *btnSave             = "Save current logs to a file";
inline constexpr const char *btnOpen             = "Open a saved log file";

// Mark log panel
inline constexpr const char *btnClearAllMarked   = "Clear all marked log entries";

// SDK tab
inline constexpr const char *btnAddProperty      = "Add the selected property to the watch list";
inline constexpr const char *btnClearAllProps    = "Remove all properties from the watch list";
inline constexpr const char *btnFetchPropertyDefs = "Fetch property definitions from the connected device";

} // namespace Tooltips

#endif // TOOLTIPS_H
