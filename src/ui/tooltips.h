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
    "Start or stop streaming `adb logcat` from the chosen device.\n"
    "A device that is not online yet is waited for: the capture starts the\n"
    "moment it answers, early enough to catch its boot log.\n"
    "Disabled while a kernel-log capture is active.";
inline constexpr const char *btnFollowReboots    =
    "Applies to both Logcat and Kernel.\n"
    "Keep capturing when the device reboots or disconnects: wait for it and\n"
    "continue — from the first line of the new boot, or from where the log\n"
    "stopped when it did not reboot. The log marks each gap (tag ToolLogPro).";
inline constexpr const char *btnKernel           =
    "Start or stop streaming the kernel log via `adb shell dmesg -w`.\n"
    "A device that is not online yet is waited for, so a reboot can be\n"
    "watched from the first kernel lines of the new boot.\n"
    "Needs root on the device. Disabled while a logcat capture is active.";
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

// SDK tab — configuration_manager properties
inline constexpr const char *btnFetchPropertyDefs =
    "Reload every property and its value from the device\n"
    "(adb shell cmd configuration_manager list --json).\n"
    "Staged changes are kept.";
inline constexpr const char *txtPropertySearch   =
    "<b>Filter properties</b> &mdash; filters as you type."
    "<table cellspacing='3'>"
    "<tr><td><code>wifi</code></td><td>in the name or value</td></tr>"
    "<tr><td><code>scan | aim</code></td><td>either word</td></tr>"
    "<tr><td><code>-usb</code></td><td>leave out rows with the word</td></tr>"
    "<tr><td><code>name:code39</code></td><td>name contains</td></tr>"
    "<tr><td><code>type:enum</code></td><td>boolean, integer, enum, multichoice, char, string, blob</td></tr>"
    "<tr><td><code>value=true</code></td><td>value is exactly true (staged values count)</td></tr>"
    "<tr><td><code>default:0</code></td><td>default contains</td></tr>"
    "<tr><td><code>id=8</code></td><td>one property by id</td></tr>"
    "</table>"
    "Use <b>Show</b> to list only changed, staged, read-only&hellip; properties.";
inline constexpr const char *cmbPropertyScope    =
    "Which properties to list before the filter applies.\n"
    "\"Changed from default\" also lists staged changes.";
inline constexpr const char *btnApplyPropertyChanges =
    "Write every staged change to the device in one command (Ctrl+S).";
inline constexpr const char *btnDiscardPropertyChanges =
    "Drop every staged change. Nothing is written.";
inline constexpr const char *btnResetPropertyDefaults =
    "Reset the selected properties to their default value on the device.";
inline constexpr const char *chkApplyPropertiesImmediately =
    "Write each change as soon as it is made, instead of staging it for Apply.";

// Filter inputs
inline constexpr const char *txtLogQuery         =
    "<b>Filter</b> &mdash; press Enter to apply."
    "<table cellspacing='3'>"
    "<tr><td><code>camera wifi</code></td><td>both words, in Tag or Message</td></tr>"
    "<tr><td><code>camera | wifi</code></td><td>either word</td></tr>"
    "<tr><td><code>-chatty</code></td><td>leave out lines with the word</td></tr>"
    "<tr><td><code>\"start proc\"</code></td><td>exact phrase</td></tr>"
    "<tr><td><code>tag:Camera</code></td><td>one column: tag, msg, pkg, pid, tid</td></tr>"
    "<tr><td><code>tag=CameraService</code></td><td>exactly this tag</td></tr>"
    "<tr><td><code>camera | wifi -tag:chatty</code></td><td>| joins first: (camera or wifi), not tag chatty</td></tr>"
    "<tr><td><code>tag:(a | b)</code></td><td>group with ( )</td></tr>"
    "</table>"
    "Right-click a Tag, Package, PID or TID cell to filter by it or exclude it.";
inline constexpr const char *txtFilterSettings   =
    "<b>Filter settings</b> &mdash; filters as you type."
    "<table cellspacing='3'>"
    "<tr><td><code>wifi</code></td><td>in the name, value or namespace</td></tr>"
    "<tr><td><code>wifi | bluetooth</code></td><td>either word</td></tr>"
    "<tr><td><code>-adb</code></td><td>leave out rows with the word</td></tr>"
    "<tr><td><code>name:wifi</code></td><td>name contains</td></tr>"
    "<tr><td><code>value=1</code></td><td>value is exactly 1 (value:1 also matches 10)</td></tr>"
    "<tr><td><code>value=(0 | null)</code></td><td>value is 0 or null</td></tr>"
    "<tr><td><code>value=\"\"</code></td><td>empty value</td></tr>"
    "<tr><td><code>ns:secure</code></td><td>namespace: system, secure or global</td></tr>"
    "</table>";
inline constexpr const char *txtFilterProperties =
    "<b>Filter properties</b> &mdash; filters as you type."
    "<table cellspacing='3'>"
    "<tr><td><code>bluetooth</code></td><td>in the name or value</td></tr>"
    "<tr><td><code>wifi | bluetooth</code></td><td>either word</td></tr>"
    "<tr><td><code>-vendor</code></td><td>leave out rows with the word</td></tr>"
    "<tr><td><code>name:ro.build</code></td><td>name contains</td></tr>"
    "<tr><td><code>value=true</code></td><td>value is exactly true</td></tr>"
    "<tr><td><code>value=(0 | false)</code></td><td>value is 0 or false</td></tr>"
    "<tr><td><code>value=\"\"</code></td><td>empty value</td></tr>"
    "</table>";
inline constexpr const char *txtStartTime        =
    "Lower bound on the log timestamp (HH:MM:SS or HH:MM:SS.mmm).";
inline constexpr const char *txtEndTime          =
    "Upper bound on the log timestamp (HH:MM:SS or HH:MM:SS.mmm).";

} // namespace Tooltips

#endif // TOOLTIPS_H
