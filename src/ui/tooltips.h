#ifndef TOOLTIPS_H
#define TOOLTIPS_H

// ---------------------------------------------------------------------------
// Central tooltip registry for ToolLogPro
// Add / update all button and widget tooltips here.
// Applied at runtime by UiManager::setupTooltips().
//
// Style guide:
//   • Start with one short imperative sentence describing the action.
//   • Add a second line with the keyboard shortcut in parentheses if any.
//   • For inputs that accept a syntax (filters), include a brief example.
// ---------------------------------------------------------------------------

namespace Tooltips {

// Top bar
inline constexpr const char *btnAppSettings      =
    "Open application settings (font, columns, ADB path, theme).";

// ADB Logcat toolbar
inline constexpr const char *btnStart            =
    "Start or stop streaming `adb logcat` from the selected device.\n"
    "Disabled while a kernel-log capture is active.";
inline constexpr const char *btnKernel           =
    "Start or stop streaming the kernel log via `adb shell dmesg -w`.\n"
    "Disabled while a logcat capture is active.";
inline constexpr const char *btnAutoScroll       =
    "Automatically scroll to the newest log line as it arrives.\n"
    "Disable to keep the current row visible.";
inline constexpr const char *btnColumns          =
    "Choose which columns are visible in the log table.\n"
    "Hidden columns still receive incoming data.";
inline constexpr const char *btnClear            =
    "Remove all log lines from the in-memory buffer and the table.\n"
    "Marked logs in the side panel are not affected.";
inline constexpr const char *btnSave             =
    "Save the current log buffer to a text file.";
inline constexpr const char *btnOpen             =
    "Load logs from a previously saved file (threadtime or brief format).";

// Mark log panel
inline constexpr const char *btnClearAllMarked   =
    "Remove every entry from the marked-log side panel.\n"
    "Original logs are left intact.";

// SDK tab
inline constexpr const char *btnAddProperty      =
    "Add the selected property definition to the watch list.";
inline constexpr const char *btnClearAllProps    =
    "Remove every property from the watch list.\n"
    "The catalog of available definitions on the device is not affected.";
inline constexpr const char *btnFetchPropertyDefs =
    "Fetch the latest values of all watched properties from the connected device.";

// Filter inputs
inline constexpr const char *txtKeyword          =
    "Live keyword filter (regular expression, case-insensitive).\n"
    "Matches against tag, message, or package.\n"
    "Example: error|warn";
inline constexpr const char *txtTagFilter        =
    "Filter by tag.  Use `,` for OR and `&` for AND.\n"
    "Example: ActivityManager,WindowManager";
inline constexpr const char *txtPidFilter        =
    "Filter by PID.  Use `,` for OR and `&` for AND.\n"
    "Example: 1234,5678";
inline constexpr const char *txtPackageFilter    =
    "Filter by package name.  Use `,` for OR.\n"
    "Example: com.android.systemui";
inline constexpr const char *txtFindMessage      =
    "Filter the message column.  Use `,` for OR and `&` for AND.\n"
    "Example: started & service";
inline constexpr const char *txtStartTime        =
    "Lower bound on the log timestamp (HH:MM:SS or HH:MM:SS.mmm).";
inline constexpr const char *txtEndTime          =
    "Upper bound on the log timestamp (HH:MM:SS or HH:MM:SS.mmm).";

} // namespace Tooltips

#endif // TOOLTIPS_H
