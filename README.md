# ToolLogPro

A Qt 6 desktop application for Android developers on Linux. Provides a rich GUI for real-time logcat streaming, log file analysis, device configuration management, and ADB tooling — all in one window.

---

## Main Features

### Log Viewing & Streaming
- **Real-time logcat streaming** via `adb logcat -v threadtime` — lines are buffered client-side and flushed to the UI in 100 ms batches to prevent UI freezes under high log volume
- **File loading** — open saved logcat files in the background (`QtConcurrent`) so the UI stays responsive; auto-detects format by trying each registered converter and picking the best parse rate
- **Save to file** — export currently loaded logs to a text file
- **Pause / Resume** — toggle log ingestion without stopping the underlying ADB process
- **Clear** — wipe the current log view and internal buffers

### Filtering
- **Multi-field filtering**: message, tag, package, PID, TID, keyword, time range, log level
- **AND / OR operators** per field — use `&&` for AND, `||` or `|` for OR within a single filter string
- **Minimum level filter** — V / D / I / W / E / A threshold
- **Time range filter** — start time and end time
- **Keyword regex filter** — matches across tag, message, and package simultaneously
- **Filter history** — Up/Down arrow keys navigate previously entered filter values (up to 50 per field)
- **Settings and Properties filter** — separate name/value AND-OR search for those tabs

### Highlighting
- **Keyword highlighting** — enter keywords; each is assigned a distinct background color
- **Highlighted text rendering** is drawn directly inside the table cells by a custom delegate (no HTML overhead)
- **Highlight navigation** — jump to next/previous highlighted match with arrow buttons
- **Per-column delegates** — independent highlight delegates for PID, package, tag, and message columns
- **Log level color coding** — each row is background-colored by level (V/D/I/W/E/A)
- **Marked row indicator** — rows added to the Marked Logs panel are visually distinguished in the main table

### Marked Logs
- Double-click or context menu to mark/unmark any log row
- Marked logs appear in a dedicated bottom panel
- **Anchor row** — select any marked log as T₀; the DELTA column shows time elapsed from that anchor for every other marked entry
- Remove individual marks or clear all

### Device Management
- **Auto-discovery** — polls `adb devices -l` every 2 seconds; UI updates only when the device list actually changes
- **Device selector** — switch between multiple connected devices; ADB streams restart automatically
- **Kernel log (dmesg)** — stream `adb shell dmesg -w`; correctly handles permission-denied vs generic errors, and suppresses false error messages on user-initiated stop

### Configuration Tab (Settings & Properties)
- **Fetch** all Android settings (global, system, secure namespaces) and all system properties from the selected device
- **Inline editing** — edit a value directly in the table; save triggers `adb shell settings put` or `adb shell setprop`, then immediately reads back the value to verify
- Per-row save result reporting with color feedback (success / failure)

### SDK / PropertyDefinition Tab
- Load, search, and display `PropertyDefinition` objects from `cmd configuration_manager get`
- **Get / Set / Remove** individual property definition values via buttons per row
- Supports `needReboot`, `readOnly`, `type` metadata fields
- Add custom property definitions and remove them from the tracked set

### Dumpsys Tab
- Run `adb shell dumpsys [service] [args]` and display the raw output
- **Service list** loaded from `adb shell dumpsys -l`
- **Text search** with next/previous navigation and inline highlighting

### Cradle Manager Tab
- Dedicated ADB commands for cradle hardware: Get Info, Query Firmware, Update Firmware, Query Schedule
- All results displayed in a dedicated output area

### Embedded Terminal
- **QTermWidget** (bundled as a third-party sub-project) provides a full terminal emulator embedded in a resizable splitter panel
- Toggle show/hide without losing terminal session state

### App Settings Dialog
- Font selector (family + size, with live preview)
- Column visibility toggles for the main log table
- Column visibility toggles for the PropertyDefinition table

---

## Architecture

```
ToolLogPro
├── src/
│   ├── main.cpp                 Entry point
│   ├── data/                    Plain data structures (no logic)
│   ├── interfaces/              Abstract base classes (contracts)
│   ├── converters/              ILogConverter implementations
│   ├── filters/                 ILogFilter / IConfigFilter implementations
│   ├── managers/                Stateful service singletons and helpers
│   ├── models/                  QAbstractTableModel subclasses
│   ├── delegates/               QStyledItemDelegate subclasses
│   └── ui/                      Widgets and dialogs
└── third_party/
    └── qtermwidget/             Embedded terminal widget (built as sub-project)
```

The application follows a **Model-View pattern** with thin UI code: the main window owns models and managers, wires signals/slots, and delegates all heavy work to background threads or service classes.

---

## Folder Breakdown

### `src/data/`
Pure structs with no methods beyond `isValid()`. Passed by value or `QVector` through the whole stack.

| Struct | Purpose |
|---|---|
| `LogEntry` | One parsed log line: id, date, time, pid, tid, package, level, tag, message |
| `SettingEntry` | One row from `adb shell settings list` |
| `PropertyEntry` | One row from `adb shell getprop` |
| `PropertyDefinition` | One entry from `cmd configuration_manager get`, includes type, readOnly, needReboot |
| `TableConfig` | `constexpr` column index and default width constants for every table |

### `src/interfaces/`
Abstract contracts that decouple implementations from the rest of the codebase.

| Interface | Purpose |
|---|---|
| `ILogConverter` | `convert(line) → LogEntry` + `name()` + `formatDescription()` |
| `ILogFilter` | `passesFilter(entry, criteria) → bool` |
| `IConfigFilter` | `passesFilter(name, value, criteria) → bool` |

`FilterCriteria` and `ParsedFilter` live here too — `ParsedFilter::build()` splits a raw filter string on `&&` / `\|\|` at construction time so the hot-path `passesFilter` loop does zero string splitting.

### `src/converters/`
Stateless converter implementations. Each knows one log format.

| Converter | Format |
|---|---|
| `ThreadTimeLogConverter` | Standard `MM-DD HH:MM:SS.mmm  PID  TID  LEVEL TAG: msg` and package variant |
| `BriefLogConverter` | `LEVEL/TAG(PID): msg` — injects current time since format has no timestamp |
| `PropertyDefinitionConverter` | Parses multi-line `cmd configuration_manager get` output |

`FileManager::readFromFileAuto()` tries all converters and picks whichever produces the highest parse success rate.

### `src/filters/`
Stateless filter implementations. Called on every row change; designed for zero allocation in the hot path.

| Filter | Purpose |
|---|---|
| `LogFilter` | 8-stage pipeline: keyword regex → message → tag → package → pid → tid → time range → level |
| `ConfigFilter` | Two-field (name + value) AND/OR filter for Settings and Properties tables |

### `src/managers/`
Stateful services and utilities.

| Manager | Purpose |
|---|---|
| `AdbManager` (singleton) | All ADB interaction: device polling, logcat/dmesg streaming, async fetch/save for settings, properties, property definitions, dumpsys, cradle commands |
| `AdbCommand` (namespace) | Single source of truth for every ADB command string — pure inline `QStringList` factories, no I/O |
| `FileManager` | File read/write for log files; auto-format detection; tracks line/parsed counts |
| `FilterHistoryManager` | Event filter installed on `QLineEdit`s; per-widget Up/Down history, max 50 entries |

### `src/models/`
Qt model classes bridging data to table views.

| Model | View | Notes |
|---|---|---|
| `LogModel` | Main log table | 8 columns; `setMarkedRows()` pointer enables row highlight without data copy |
| `MarkLogModel` | Marked logs panel | 9 columns (adds DELTA); anchor row mechanism for relative timestamps |
| `SettingsModel` | Settings table | Editable value column with inline action button |
| `PropertiesModel` | Properties table | Same pattern as SettingsModel |
| `PropertyDefinitionModel` | SDK table | 11 columns including Get/Set/Remove action buttons |

### `src/delegates/`

| Delegate | Purpose |
|---|---|
| `HighlightDelegate` | Paints keyword highlights as colored backgrounds on matched substrings directly in `paint()`; supports word-wrap for tall rows; cycles through a static palette |
| `ValueDelegate` | Inline editable cells for settings/properties tables |

Four `HighlightDelegate` instances are installed on separate columns (PID, package, tag, message) so keyword colors are consistent across fields.

### `src/ui/`
Qt Designer-backed widgets.

| File | Purpose |
|---|---|
| `MainWindow` | Central coordinator: owns all models, managers, delegates; wires all signals/slots; manages batch flush timer, file load watcher, splitter state, auto-scroll, status bar |
| `SettingsDialog` | App preferences: font picker, column visibility for log table and PropertyDefinition table |

`FileLoadResult` is a plain struct used as the future result type for background file loading — it carries the fully parsed `QVector<LogEntry>`, pre-built ID→index hash, and metadata so the main thread only does an `O(1)` `std::move` on arrival.

### `third_party/qtermwidget/`
Full source copy of the qtermwidget library, built as a CMake sub-project and linked statically. Provides the embedded terminal emulator in the Terminal panel.

---

## Deployment

ToolLogPro is distributed as a **self-contained directory** — no Qt installation required on the target machine.

### Build (from source)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```
Requires: Qt 6.5+, CMake 3.19+, a C++17-capable compiler.

### Package (portable binary)
```bash
cd deploy
./deploy.sh          # or ./build-appimage.sh for AppImage
```
The packaging script uses `linuxdeploy` to copy all required Qt and system libraries alongside the binary.

### Packaged structure
```
ToolLogPro/
├── ToolLogPro          # Release binary
├── run.sh              # Launcher (sets LD_LIBRARY_PATH, QT_PLUGIN_PATH)
├── qt.conf             # Qt path configuration
├── lib/                # Bundled Qt 6.9.1 + ICU + graphics libraries
└── plugins/            # Qt platform plugins (xcb, wayland, themes)
```

### Run packaged build
```bash
cd deploy/ToolLogPro
./run.sh
```

### Distribute
```bash
tar -xzf ToolLogPro-<date>.tar.gz
cd ToolLogPro && ./run.sh
```

### System requirements (target machine)
- Linux with **GLIBC 2.27+** (Ubuntu 18.04+, Debian 10+, Fedora 28+, CentOS 8+)
- X11 or Wayland display server
- Common system libraries usually pre-installed: `libdbus-1-3`, `libglib2.0-0`, `libpng16-16`

### AppImage
A standalone `ToolLogPro-x86_64.AppImage` is available in the `deploy/` directory for single-file distribution.

---

## Key Signal/Data Flow

```
AdbManager
  │  logcatLineReceived(line)          ← raw text from adb process stdout
  ▼
MainWindow
  │  buffered in m_pendingLines[]
  │  flushed every 100 ms (m_batchFlushTimer)
  ▼
ILogConverter::convert(line) → LogEntry
  ▼
allLogs[] + allLogsIndex{}            ← full history in memory
  ▼
ILogFilter::passesFilter()            ← applied on every incoming entry
  ▼
filteredLogs[] + filteredLogsIndex{}  ← what the table currently shows
  ▼
LogModel → QTableView                 ← rendered with HighlightDelegate
```

File loading takes the same path but runs on a `QtConcurrent` worker thread:
```
QFutureWatcher<FileLoadResult>
  └─ worker: parse all lines, build index
  └─ main thread (onFileLoadFinished): std::move result, replace model data
```
