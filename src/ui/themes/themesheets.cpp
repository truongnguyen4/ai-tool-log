#include "themesheets.h"
#include "components/palette.h"

namespace ThemeSheets {

using UiComponents::Palette;

// ─────────────────────────────────────────────────────────────────────────────
// fromPalette — single QSS template parameterized by design tokens.
//
// To restyle the entire app:
//   - Color/contrast/accent changes  →  edit palette_light.cpp / palette_dark.cpp
//   - Layout/spacing/radius changes  →  edit this template
//   - Add a new theme                →  add palette_<name>.cpp returning a
//                                       Palette and call fromPalette(...) here.
// ─────────────────────────────────────────────────────────────────────────────
QString fromPalette(const Palette &p)
{
    return QStringLiteral(R"(
/* ── Base ─────────────────────────────────────────────────────────────────── */
QMainWindow, QDialog { background-color: %1; color: %2; }
QWidget { background-color: %1; color: %2; }

/* ── Line Edit ────────────────────────────────────────────────────────────── */
QLineEdit {
    background-color: %3; border: 1px solid %4; border-radius: 6px;
    padding: 6px 10px; color: %2;
    selection-background-color: %5; selection-color: %6;
}
QLineEdit:hover    { border-color: %7; }
QLineEdit:focus    { border: 2px solid %8; padding: 5px 9px; background-color: %3; }
QLineEdit:disabled { color: %9; background-color: %10; border-color: %4; }

/* ── Push Button ──────────────────────────────────────────────────────────── */
QPushButton {
    background-color: %3; border: 1px solid %4; border-radius: 6px;
    padding: 6px 14px; color: %2; font-weight: 500;
}
QPushButton:hover    { background-color: %10; border-color: %7; }
QPushButton:pressed  { background-color: %11; border-color: %8; }
QPushButton:disabled { color: %9; background-color: %10; border-color: %4; }

QPushButton:checked       { background-color: %8; border: 2px solid %13; color: %12; padding: 5px 13px; }
QPushButton:checked:hover { background-color: %13; border-color: %13; }

QPushButton#btnResume       { background-color: %8; border-color: %8; color: %12; }
QPushButton#btnResume:hover { background-color: %13; border-color: %13; }
QPushButton#btnAutoScroll:checked { background-color: %8; border-color: %8; }

/* ── ComboBox ─────────────────────────────────────────────────────────────── */
QComboBox {
    background-color: %3; border: 1px solid %4; border-radius: 6px;
    padding: 6px 10px; color: %2;
}
QComboBox:hover { border-color: %7; }
QComboBox:focus { border-color: %8; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox::down-arrow {
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid %14;
    width: 0; height: 0;
}
QComboBox QAbstractItemView {
    background-color: %3; color: %2;
    selection-background-color: %5; selection-color: %6;
    border: 1px solid %4; border-radius: 6px; outline: none; padding: 4px;
}
QComboBox QAbstractItemView::item {
    background-color: transparent; color: %2;
    padding: 6px 10px; border-radius: 4px; min-height: 24px;
}
QComboBox QAbstractItemView::item:hover    { background-color: %10; }
QComboBox QAbstractItemView::item:selected { background-color: %5; color: %6; }

/* ── Table View ───────────────────────────────────────────────────────────── */
QTableView {
    background-color: %3; alternate-background-color: %15;
    gridline-color: %16; color: %2;
    border: 1px solid %4; border-radius: 6px;
    selection-background-color: %5; selection-color: %6;
}
QTableView::item              { padding: 5px 8px; border: none; }
QTableView::item:selected     { background-color: %5; color: %6; }
QTableView::item:focus        { background-color: %8; outline: none; color: %12; }
QTableView::item:hover:!selected { background-color: %10; }

QHeaderView { background-color: %10; }
QHeaderView::section {
    background-color: %10; color: %17;
    padding: 7px 10px; border: none;
    border-right: 1px solid %4; border-bottom: 1px solid %4;
    font-weight: 600;
}
QHeaderView::section:hover { background-color: %11; color: %2; }
)").arg(
        /* %1  */ p.background,
        /* %2  */ p.text,
        /* %3  */ p.surface,
        /* %4  */ p.border,
        /* %5  */ p.accentSubtle,
        /* %6  */ p.accentSubtleText,
        /* %7  */ p.borderStrong,
        /* %8  */ p.accent,
        /* %9  */ p.textSubtle)
       .arg(p.surfaceMuted, p.surfaceHover, p.textOnAccent, p.accentHover,
            p.textMuted, p.background)
       .arg(p.divider, p.textMuted)

    + QStringLiteral(R"(
/* ── RadioButton ──────────────────────────────────────────────────────────── */
QRadioButton { color: %1; spacing: 8px; }
QRadioButton::indicator { width: 16px; height: 16px; }
QRadioButton::indicator:unchecked {
    border: 1.5px solid %2; border-radius: 8px; background-color: %3;
}
QRadioButton::indicator:unchecked:hover { border-color: %4; }
QRadioButton::indicator:checked {
    border: 1.5px solid %4; border-radius: 8px; background-color: %4;
}

/* ── CheckBox ─────────────────────────────────────────────────────────────── */
QCheckBox { color: %1; spacing: 8px; }
QCheckBox::indicator {
    width: 16px; height: 16px; border: 1.5px solid %2;
    border-radius: 4px; background-color: %3;
}
QCheckBox::indicator:hover           { border-color: %4; }
QCheckBox::indicator:checked         { background-color: %4; border-color: %4; }
QCheckBox::indicator:checked:hover   { background-color: %5; border-color: %5; }

/* ── Label ────────────────────────────────────────────────────────────────── */
QLabel { color: %1; background: transparent; }

/* ── Group Box ────────────────────────────────────────────────────────────── */
QGroupBox {
    border: 1px solid %6; border-radius: 8px;
    margin-top: 14px; padding: 12px 10px 10px 10px;
    color: %1; background-color: %3; font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin; subcontrol-position: top left;
    left: 10px; padding: 0 6px; color: %7;
    font-weight: 600; font-size: 11px;
    text-transform: uppercase; letter-spacing: 0.5px;
}

/* ── Splitter ─────────────────────────────────────────────────────────────── */
QSplitter::handle            { background-color: %6; }
QSplitter::handle:horizontal { width: 1px; }
QSplitter::handle:vertical   { height: 1px; }
QSplitter::handle:hover      { background-color: %4; }

/* ── Scroll Bars ──────────────────────────────────────────────────────────── */
QScrollBar:vertical { border: none; background-color: transparent; width: 10px; }
QScrollBar::handle:vertical {
    background-color: %8; border-radius: 5px; min-height: 28px; margin: 2px;
}
QScrollBar::handle:vertical:hover   { background-color: %9; }
QScrollBar::handle:vertical:pressed { background-color: %10; }

QScrollBar:horizontal { border: none; background-color: transparent; height: 10px; }
QScrollBar::handle:horizontal {
    background-color: %8; border-radius: 5px; min-width: 28px; margin: 2px;
}
QScrollBar::handle:horizontal:hover { background-color: %9; }

QScrollBar::add-line, QScrollBar::sub-line { border: none; background: none; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
)").arg(
        /* %1  */ p.text,
        /* %2  */ p.textSubtle,
        /* %3  */ p.surface,
        /* %4  */ p.accent,
        /* %5  */ p.accentHover,
        /* %6  */ p.border,
        /* %7  */ p.textSubtle,
        /* %8  */ p.scrollbarHandle,
        /* %9  */ p.scrollbarHandleHover)
       .arg(p.scrollbarHandleActive)

    + QStringLiteral(R"(
/* ── Tab Widget ───────────────────────────────────────────────────────────── */
QTabWidget::pane {
    border: none; border-top: 1px solid %1; background-color: %2;
}
QTabWidget::tab-bar { left: 0; }

QTabBar { background-color: %3; }
QTabBar::tab {
    background-color: transparent; color: %4;
    border: none; border-bottom: 2px solid transparent;
    padding: 10px 18px; font-weight: 500;
}
QTabBar::tab:hover:!selected { color: %5; background-color: %6; }
QTabBar::tab:selected        { color: %7; border-bottom: 2px solid %7; font-weight: 600; }

QTabBar::tab:left {
    min-width: 120px; min-height: 42px;
    border-bottom: none; border-right: 2px solid transparent;
    padding: 12px 16px; text-align: left;
}
QTabBar::tab:left:selected        { border-right: 2px solid %7; border-bottom: none; color: %7; }
QTabBar::tab:left:hover:!selected { background-color: %6; color: %5; }

/* ─── Main navigation rail (tabWidget) ─────────────────────────────────── */
QTabWidget#tabWidget::pane {
    border: none;
    border-left: 1px solid %1;
    background-color: %2;
}
QTabWidget#tabWidget::tab-bar {
    left: 0;
    top: 0;
}
QTabWidget#tabWidget QTabBar {
    background-color: %3;
    qproperty-drawBase: 0;
}
QTabWidget#tabWidget QTabBar::tab {
    background-color: transparent;
    color: %4;
    border: none;
    border-right: 3px solid transparent;
    padding: 0;
    margin: 0 0 2px 0;
    min-width: 52px;
    max-width: 52px;
    min-height: 52px;
    max-height: 52px;
}
QTabWidget#tabWidget QTabBar::tab:hover:!selected {
    color: %5;
    background-color: %6;
}
QTabWidget#tabWidget QTabBar::tab:selected {
    color: %7;
    border-right: 3px solid %7;
    background-color: %2;
}
QTabWidget#tabWidget QTabBar::tab:!selected:!hover {
    background-color: transparent;
}

/* ── Plain / Text Edit ────────────────────────────────────────────────────── */
QPlainTextEdit, QTextEdit {
    background-color: %2; border: 1px solid %1; border-radius: 6px;
    color: %5; selection-background-color: %8; selection-color: %9; padding: 4px;
}
QPlainTextEdit:focus, QTextEdit:focus { border-color: %7; }

/* ── Status Bar ───────────────────────────────────────────────────────────── */
QStatusBar {
    background-color: %3;
    color: %4;
    border-top: 1px solid %1;
    padding: 4px 12px;
    min-height: 26px;
    font-size: 11px;
    font-weight: 500;
    letter-spacing: 0.3px;
}
QStatusBar::item { border: none; }
QStatusBar QLabel { color: %4; padding: 0 6px; }
QStatusBar QPushButton {
    background: transparent;
    color: %5;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 2px 8px;
    font-weight: 600;
}
QStatusBar QPushButton:hover { background-color: %6; border-color: %1; }
QStatusBar QPushButton:pressed { background-color: %2; }
QStatusBar QProgressBar {
    background: %2;
    border: 1px solid %1;
    border-radius: 4px;
    height: 6px;
    text-align: center;
}
QStatusBar QProgressBar::chunk {
    background-color: %7;
    border-radius: 4px;
}

/* ── Tooltips ─────────────────────────────────────────────────────────────── */
QToolTip {
    background-color: %10; color: %11; border: 1px solid %12;
    border-radius: 6px; padding: 6px 10px; font-size: 12px;
}

/* ── Menus ────────────────────────────────────────────────────────────────── */
QMenuBar { background-color: %2; color: %5; padding: 2px; }
QMenuBar::item { padding: 4px 10px; border-radius: 4px; }
QMenuBar::item:selected { background-color: %3; }
QMenu {
    background-color: %13; color: %5; border: 1px solid %1;
    border-radius: 8px; padding: 4px;
}
QMenu::item { padding: 6px 14px; border-radius: 4px; }
QMenu::item:selected { background-color: %14; color: %15; }
QMenu::separator { height: 1px; background: %1; margin: 4px 6px; }

/* ── List & Tree ──────────────────────────────────────────────────────────── */
QListView, QTreeView {
    background-color: %2; color: %5; border: 1px solid %1; border-radius: 6px;
    selection-background-color: %14; selection-color: %15; padding: 4px;
}
QListView::item, QTreeView::item { padding: 6px 8px; border-radius: 4px; }
QListView::item:hover, QTreeView::item:hover { background-color: %3; }

/* ── Spin Box ─────────────────────────────────────────────────────────────── */
QSpinBox, QDoubleSpinBox {
    background-color: %13; border: 1px solid %1; border-radius: 6px;
    padding: 6px 10px; color: %5; selection-background-color: %8;
}
QSpinBox:hover, QDoubleSpinBox:hover { border-color: %16; }
QSpinBox:focus, QDoubleSpinBox:focus { border-color: %7; }
)").arg(
        /* %1  */ p.border,
        /* %2  */ p.surfaceMuted,
        /* %3  */ p.surfaceMuted,
        /* %4  */ p.textMuted,
        /* %5  */ p.text,
        /* %6  */ p.surfaceHover,
        /* %7  */ p.accent,
        /* %8  */ p.accentSubtle,
        /* %9  */ p.accentSubtleText)
       .arg(p.tooltipBg, p.tooltipText, p.tooltipBorder, p.surface,
            p.accentSubtle, p.accentSubtleText)
       .arg(p.borderStrong)

    + QStringLiteral(R"(
/* ── Log level radio colors ───────────────────────────────────────────────── */
QRadioButton#radioVerbosePlus, QRadioButton#radioV { color: %1; }
QRadioButton#radioD { color: %2; }
QRadioButton#radioI { color: %3; }
QRadioButton#radioW { color: %4; }
QRadioButton#radioE { color: %5; }
QRadioButton#radioA { color: %6; }

QRadioButton#radioVerbosePlus::indicator:checked,
QRadioButton#radioV::indicator:checked { background-color: %1; border-color: %1; }
QRadioButton#radioD::indicator:checked { background-color: %2; border-color: %2; }
QRadioButton#radioI::indicator:checked { background-color: %3; border-color: %3; }
QRadioButton#radioW::indicator:checked { background-color: %4; border-color: %4; }
QRadioButton#radioE::indicator:checked { background-color: %5; border-color: %5; }
QRadioButton#radioA::indicator:checked { background-color: %6; border-color: %6; }

/* ═════════════════════════════════════════════════════════════════════════════
 * UiComponents library — property-driven variants
 * ═════════════════════════════════════════════════════════════════════════════ */

/* ── Button variants ──────────────────────────────────────────────────────── */
QPushButton[variant="primary"] {
    background-color: %7; border: 1px solid %7; color: %8;
    border-radius: 6px; padding: 7px 16px; font-weight: 600;
}
QPushButton[variant="primary"]:hover    { background-color: %9; border-color: %9; }
QPushButton[variant="primary"]:pressed  { background-color: %10; border-color: %10; }

QPushButton[variant="secondary"] {
    background-color: %11; border: 1px solid %12; color: %13;
    border-radius: 6px; padding: 7px 16px; font-weight: 500;
}
QPushButton[variant="secondary"]:hover   { background-color: %14; border-color: %15; }
QPushButton[variant="secondary"]:pressed { background-color: %16; border-color: %7; }

QPushButton[variant="ghost"] {
    background-color: transparent; border: 1px solid transparent; color: %17;
    border-radius: 6px; padding: 7px 14px;
}
QPushButton[variant="ghost"]:hover   { background-color: %14; color: %13; }
QPushButton[variant="ghost"]:pressed { background-color: %16; }

QPushButton[variant="danger"] {
    background-color: %18; border: 1px solid %18; color: %8;
    border-radius: 6px; padding: 7px 16px; font-weight: 600;
}
QPushButton[variant="danger"]:hover    { background-color: %19; border-color: %19; }
QPushButton[variant="danger"]:pressed  { background-color: %19; border-color: %19; }

QPushButton[variant="icon"] {
    background-color: transparent; border: 1px solid transparent; color: %17;
    border-radius: 6px; padding: 4px; min-width: 28px;
}
QPushButton[variant="icon"]:hover    { background-color: %14; color: %13; }
QPushButton[variant="icon"]:pressed  { background-color: %16; }
QPushButton[variant="icon"]:checked  { background-color: %20; color: %21; }

QPushButton[size="sm"] { padding: 4px 10px; min-height: 22px; font-size: 12px; }
QPushButton[size="md"] { min-height: 28px; }
QPushButton[size="lg"] { padding: 9px 20px; min-height: 36px; font-size: 14px; }

/* Monitor button: square icon toggle, vivid danger-red while polling. */
QPushButton[role="monitor"] {
    background-color: %11; border: 1px solid %12; color: %13;
    border-radius: 6px; padding: 0;
}
QPushButton[role="monitor"]:hover { background-color: %14; border-color: %15; }
QPushButton[role="monitor"]:checked {
    background-color: %18; border: 1px solid %18; color: %8;
    font-weight: 700;
}
QPushButton[role="monitor"]:checked:hover   { background-color: %19; border-color: %19; }
QPushButton[role="monitor"]:checked:pressed { background-color: %19; border-color: %19; }

/* Preset chips in the dumpsys toolbar: compact, pill-shaped ghost buttons. */
QPushButton[role="chip"] {
    border-radius: 11px; padding: 3px 12px; min-height: 20px;
    font-size: 11px; color: %17; background-color: %11; border: 1px solid %12;
}
QPushButton[role="chip"]:hover   { background-color: %14; color: %13; border-color: %15; }
QPushButton[role="chip"]:pressed { background-color: %16; }

/* ── Active-pane / live-monitor accent ────────────────────────────────────────
 * Marks the pane the user is driving in a split log view, and any config table
 * that is currently being re-fetched on a timer. Both states reserve the same
 * 2px so switching never shifts the layout.
 *
 * These selectors repeat the object id on purpose: the per-table rules further
 * down are id-selectors, which outrank a bare attribute selector in QSS. */
QTableView#tableLog[pane="active"],     QTableView#tableMarkLog[pane="active"],
QTableView#tableLogB[pane="active"],    QTableView#tableMarkLogB[pane="active"],
QTableView#tableSettings[pane="active"], QTableView#tableProperties[pane="active"],
QTableView#tablePropertyDefinitions[pane="active"] {
    border: 2px solid %7; border-radius: 4px;
}
QTableView#tableLog[pane="inactive"],  QTableView#tableMarkLog[pane="inactive"],
QTableView#tableLogB[pane="inactive"], QTableView#tableMarkLogB[pane="inactive"] {
    border: 2px solid transparent; border-radius: 4px;
}
QTableView#tableSettings[pane="inactive"], QTableView#tableProperties[pane="inactive"],
QTableView#tablePropertyDefinitions[pane="inactive"] {
    border: 2px solid %12; border-radius: 4px;
}

/* Capture toggles (Logcat / Kernel) while a capture is running. */
QPushButton#btnStart[state="recording"], QPushButton#btnKernel[state="recording"] {
    background-color: %18; border: 1px solid %18; color: %8; font-weight: 600;
}
QPushButton#btnStart[state="recording"]:hover,
QPushButton#btnKernel[state="recording"]:hover {
    background-color: %19; border-color: %19;
}

/* Toolbar group separators. */
QFrame#toolbarDivider { color: %12; background: %12; border: none; margin: 2px 6px; }

/* Devices tab: one row per connected device in the sidebar list. */
QWidget[deviceRow="true"] {
    background-color: transparent; border-left: 2px solid transparent;
}
QWidget[deviceRow="true"][selected="true"] {
    background-color: %20; border-left: 2px solid %7;
}

/* Firmware-flash log output. */
QTextEdit#flashOutputView {
    background-color: %11; color: %13; border: 1px solid %12; border-radius: 6px;
    font-family: "JetBrains Mono","Cascadia Code","Consolas","Courier New",monospace;
    font-size: 12px;
}

/* Device-connection indicator in the Logcat toolbar. */
QLabel#lblDeviceStatus                        { font-size: 16px; }
QLabel#lblDeviceStatus[state="connected"]     { color: %22; }
QLabel#lblDeviceStatus[state="disconnected"]  { color: %18; }

/* ── Input variants ───────────────────────────────────────────────────────── */
QLineEdit[variant="search"]   { padding-left: 12px; padding-right: 12px; border-radius: 16px; }
QLineEdit[variant="password"] { font-family: "monospace"; letter-spacing: 2px; }
QLineEdit[size="sm"] { padding: 3px 8px; min-height: 20px; font-size: 12px; }
QLineEdit[size="md"] { min-height: 26px; }
QLineEdit[size="lg"] { padding: 9px 12px; min-height: 34px; font-size: 14px; }

/* ── Card variants (QGroupBox) ────────────────────────────────────────────── */
QGroupBox[variant="card"] {
    background-color: %11; border: 1px solid %12; border-radius: 8px;
    margin-top: 14px; padding: 12px 12px 10px 12px;
}
QGroupBox[variant="section"] {
    background-color: %11; border: 1px solid %12; border-radius: 10px;
    margin-top: 16px; padding: 16px 14px 12px 14px;
}
QGroupBox[variant="section"]::title {
    color: %13; font-weight: 700; font-size: 12px;
    text-transform: uppercase; letter-spacing: 0.6px;
}
QGroupBox[variant="flat"] {
    background-color: transparent; border: none; margin-top: 8px; padding: 4px 0;
}
QGroupBox[variant="flat"]::title { color: %17; }

/* ── Label roles ──────────────────────────────────────────────────────────── */
QLabel[role="h1"]      { color: %13; font-size: 22px; font-weight: 700; }
QLabel[role="h2"]      { color: %13; font-size: 17px; font-weight: 600; }
QLabel[role="h3"]      { color: %13; font-size: 14px; font-weight: 600; }
QLabel[role="body"]    { color: %13; font-size: 12px; }
QLabel[role="caption"] { color: %17; font-size: 11px; }
QLabel[role="mono"]    { color: %13; font-family: "JetBrains Mono","Fira Code","Cascadia Code","Menlo",monospace; font-size: 12px; }

/* ═════════════════════════════════════════════════════════════════════════════
 * Android log tab — modern table styling
 * ═════════════════════════════════════════════════════════════════════════════ */

/* Log tables: borderless, taller rows, soft alternating bands */
QTableView#tableLog, QTableView#tableMarkLog,
QTableView#tableLogB, QTableView#tableMarkLogB {
    background-color: %11;
    alternate-background-color: %16;
    gridline-color: transparent;
    border: none;
    selection-background-color: %20;
    selection-color: %21;
    font-size: 12px;
}
QTableView#tableLog::item, QTableView#tableMarkLog::item,
QTableView#tableLogB::item, QTableView#tableMarkLogB::item {
    padding: 4px 10px;
    border: none;
    border-bottom: 1px solid %12;
}
QTableView#tableLog::item:selected, QTableView#tableMarkLog::item:selected,
QTableView#tableLogB::item:selected, QTableView#tableMarkLogB::item:selected {
    background-color: %20; color: %21;
}
QTableView#tableLog::item:hover:!selected, QTableView#tableMarkLog::item:hover:!selected,
QTableView#tableLogB::item:hover:!selected, QTableView#tableMarkLogB::item:hover:!selected {
    background-color: %14;
}

/* Log table headers: cleaner, more uppercase-y */
QTableView#tableLog QHeaderView::section,
QTableView#tableMarkLog QHeaderView::section,
QTableView#tableLogB QHeaderView::section,
QTableView#tableMarkLogB QHeaderView::section {
    background-color: %16;
    color: %17;
    padding: 8px 12px;
    border: none;
    border-right: 1px solid %12;
    border-bottom: 2px solid %12;
    font-weight: 600;
    font-size: 11px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
}
QTableView#tableLog QHeaderView::section:hover,
QTableView#tableMarkLog QHeaderView::section:hover,
QTableView#tableLogB QHeaderView::section:hover,
QTableView#tableMarkLogB QHeaderView::section:hover {
    background-color: %14; color: %13;
}

/* ─── Configuration & SDK tab tables (apply same modern look) ─── */
QTableView#tableSettings, QTableView#tableProperties,
QTableView#tablePropertyDefinitions {
    background-color: %11;
    alternate-background-color: %16;
    gridline-color: transparent;
    border: 1px solid %12;
    border-radius: 8px;
    selection-background-color: %20;
    selection-color: %21;
    font-size: 12px;
    padding: 0px;
}
QTableView#tableSettings::item, QTableView#tableProperties::item,
QTableView#tablePropertyDefinitions::item {
    padding: 6px 10px;
    border: none;
    border-bottom: 1px solid %12;
}
QTableView#tableSettings::item:selected, QTableView#tableProperties::item:selected,
QTableView#tablePropertyDefinitions::item:selected {
    background-color: %20; color: %21;
}
QTableView#tableSettings::item:hover:!selected,
QTableView#tableProperties::item:hover:!selected,
QTableView#tablePropertyDefinitions::item:hover:!selected {
    background-color: %14;
}
QTableView#tableSettings QHeaderView::section,
QTableView#tableProperties QHeaderView::section,
QTableView#tablePropertyDefinitions QHeaderView::section {
    background-color: %16;
    color: %17;
    padding: 9px 12px;
    border: none;
    border-right: 1px solid %12;
    border-bottom: 2px solid %12;
    font-weight: 600;
    font-size: 11px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
}
QTableView#tableSettings QHeaderView::section:hover,
QTableView#tableProperties QHeaderView::section:hover,
QTableView#tablePropertyDefinitions QHeaderView::section:hover {
    background-color: %14; color: %13;
}

/* ─── Section headers above tables (Settings / Properties / SDK) ─── */
QLabel#lblSettings, QLabel#lblProperties {
    color: %13;
    font-weight: 700;
    font-size: 13px;
    letter-spacing: 0.6px;
}

/* Filter input row */
QLineEdit#txtFilterSettings, QLineEdit#txtFilterSettingsValue,
QLineEdit#txtFilterProperties, QLineEdit#txtFilterPropertiesValue {
    background-color: %16;
    color: %13;
    border: 1px solid %12;
    border-radius: 6px;
    padding: 6px 10px;
    selection-background-color: %20;
    selection-color: %21;
}
QLineEdit#txtFilterSettings:hover, QLineEdit#txtFilterSettingsValue:hover,
QLineEdit#txtFilterProperties:hover, QLineEdit#txtFilterPropertiesValue:hover {
    border-color: %15;
}
QLineEdit#txtFilterSettings:focus, QLineEdit#txtFilterSettingsValue:focus,
QLineEdit#txtFilterProperties:focus, QLineEdit#txtFilterPropertiesValue:focus {
    border-color: %7;
}

/* Refresh icon buttons next to section headers */
QPushButton#btnRefreshSettings, QPushButton#btnRefreshProperties {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 4px;
}
QPushButton#btnRefreshSettings:hover, QPushButton#btnRefreshProperties:hover {
    background-color: %14; border-color: %12;
}
QPushButton#btnRefreshSettings:pressed, QPushButton#btnRefreshProperties:pressed {
    background-color: %16;
}

/* ─── SDK tab — toolbar + inner tab widget ─── */
QTabWidget#tabWidgetSDK::pane {
    border: none;
    border-top: 1px solid %12;
    background-color: %11;
}
QTabWidget#tabWidgetSDK QTabBar { background-color: %16; }
QTabWidget#tabWidgetSDK QTabBar::tab {
    background-color: transparent;
    color: %17;
    border: none;
    border-bottom: 2px solid transparent;
    padding: 9px 18px;
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 0.4px;
    text-transform: uppercase;
}
QTabWidget#tabWidgetSDK QTabBar::tab:hover:!selected {
    color: %13; background-color: %14;
}
QTabWidget#tabWidgetSDK QTabBar::tab:selected {
    color: %13;
    border-bottom: 2px solid %7;
    background-color: %11;
}

/* ─── SDK sidebar (vertical "tabs" via QListWidget + QStackedWidget) ─── */
QWidget#sdkSidebarContainer { background-color: %11; }
QListWidget#sdkSidebar {
    background-color: %16;
    border: none;
    border-right: 1px solid %12;
    padding: 8px 0;
    outline: none;
}
QListWidget#sdkSidebar::item {
    color: %17;
    padding: 10px 18px;
    border: none;
    border-left: 3px solid transparent;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 0.3px;
}
QListWidget#sdkSidebar::item:hover:!selected {
    color: %13;
    background-color: %14;
}
QListWidget#sdkSidebar::item:selected {
    color: %13;
    background-color: %11;
    border-left: 3px solid %7;
}
QStackedWidget#sdkStack { background-color: %11; }

/* ─── Settings dialog sidebar (QListWidget + QStackedWidget) ─── */
QListWidget#settingsSidebar {
    background-color: %16;
    border: none;
    border-right: 1px solid %12;
    padding: 8px 0;
    outline: none;
}
QListWidget#settingsSidebar::item {
    color: %17;
    padding: 10px 18px;
    border: none;
    border-left: 3px solid transparent;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 0.3px;
}
QListWidget#settingsSidebar::item:hover:!selected {
    color: %13;
    background-color: %14;
}
QListWidget#settingsSidebar::item:selected {
    color: %13;
    background-color: %11;
    border-left: 3px solid %7;
}
QStackedWidget#settingsStack { background-color: %11; }
QWidget#settingsFooter {
    background-color: %16;
    border-top: 1px solid %12;
}

QLabel#lblSDKTitle {
    color: %13;
    font-weight: 700;
    font-size: 12px;
    letter-spacing: 0.8px;
    padding: 4px 0;
}

/* SDK toolbar icon buttons (Export/Import/Save/Load + Refresh/Clear) */
QPushButton#btnExportPropertySet, QPushButton#btnImportPropertySet,
QPushButton#btnSavePropertySet,   QPushButton#btnLoadPropertySet,
QPushButton#btnFetchPropertyDefs, QPushButton#btnClearAllProperties {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 4px;
}
QPushButton#btnExportPropertySet:hover, QPushButton#btnImportPropertySet:hover,
QPushButton#btnSavePropertySet:hover,   QPushButton#btnLoadPropertySet:hover,
QPushButton#btnFetchPropertyDefs:hover, QPushButton#btnClearAllProperties:hover {
    background-color: %14; border-color: %12;
}
QPushButton#btnExportPropertySet:pressed, QPushButton#btnImportPropertySet:pressed,
QPushButton#btnSavePropertySet:pressed,   QPushButton#btnLoadPropertySet:pressed,
QPushButton#btnFetchPropertyDefs:pressed, QPushButton#btnClearAllProperties:pressed {
    background-color: %16;
}

/* SDK property search field */
QLineEdit#txtPropertySearch {
    background-color: %16;
    color: %13;
    border: 1px solid %12;
    border-radius: 6px;
    padding: 7px 12px;
    selection-background-color: %20;
    selection-color: %21;
}
QLineEdit#txtPropertySearch:hover { border-color: %15; }
QLineEdit#txtPropertySearch:focus { border-color: %7; }

/* ─── ADB Logcat tab toolbar — modern ─── */
QPushButton#btnStart {
    background-color: %7; color: %8;
    border: none; border-radius: 6px;
    padding: 7px 16px; font-weight: 700;
    letter-spacing: 0.4px;
}
QPushButton#btnStart:hover    { background-color: %9; }
QPushButton#btnStart:pressed  { background-color: %10; }
QPushButton#btnStart:disabled { background-color: %16; color: %17; }

QPushButton#btnKernel, QPushButton#btnAutoScroll, QPushButton#btnColumns,
QPushButton#btnClear, QPushButton#btnSave,
QPushButton#btnOpen, QPushButton#btnAppSettings {
    background-color: %16;
    color: %13;
    border: 1px solid %12;
    border-radius: 6px;
    padding: 6px 12px;
    font-weight: 600;
}
QPushButton#btnKernel:hover, QPushButton#btnAutoScroll:hover,
QPushButton#btnColumns:hover,
QPushButton#btnClear:hover, QPushButton#btnSave:hover, QPushButton#btnOpen:hover,
QPushButton#btnAppSettings:hover {
    background-color: %14; border-color: %7;
}
QPushButton#btnKernel:pressed, QPushButton#btnAutoScroll:pressed,
QPushButton#btnColumns:pressed,
QPushButton#btnClear:pressed, QPushButton#btnSave:pressed, QPushButton#btnOpen:pressed,
QPushButton#btnAppSettings:pressed {
    background-color: %16;
}
QPushButton#btnKernel:checked, QPushButton#btnAutoScroll:checked {
    background-color: %20; color: %21; border-color: %7;
}

/* Highlight nav buttons */
QPushButton#btnHighlightPrev, QPushButton#btnHighlightNext,
QPushButton#btnDumpsysSearchPrev, QPushButton#btnDumpsysSearchNext,
QPushButton#btnDumpsysRefresh {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 6px;
    padding: 4px;
}
QPushButton#btnHighlightPrev:hover, QPushButton#btnHighlightNext:hover,
QPushButton#btnDumpsysSearchPrev:hover, QPushButton#btnDumpsysSearchNext:hover,
QPushButton#btnDumpsysRefresh:hover {
    background-color: %14; border-color: %12;
}

/* Top device combo box */
QComboBox#cmbDevice {
    background-color: %16;
    color: %13;
    border: 1px solid %12;
    border-radius: 6px;
    padding: 6px 28px 6px 12px;
    min-width: 160px;
    font-weight: 600;
}
QComboBox#cmbDevice:hover { border-color: %15; }
QComboBox#cmbDevice:focus { border-color: %7; }
QComboBox#cmbDevice::drop-down {
    subcontrol-origin: padding; subcontrol-position: right center;
    width: 22px; border: none;
}
QComboBox#cmbDevice::down-arrow {
    image: none;
    width: 0; height: 0;
    border-left: 5px solid transparent;
    border-right: 5px solid transparent;
    border-top: 6px solid %17;
    margin-right: 8px;
}
QComboBox#cmbDevice QAbstractItemView {
    background-color: %16;
    color: %13;
    border: 1px solid %12;
    border-radius: 6px;
    padding: 4px;
    selection-background-color: %20;
    selection-color: %21;
    outline: none;
}

/* Tag/PID filter inputs in toolbar */
QLineEdit#txtTagFilter, QLineEdit#txtPidFilter {
    background-color: %16;
    color: %13;
    border: 1px solid %12;
    border-radius: 6px;
    padding: 6px 10px;
    selection-background-color: %20;
    selection-color: %21;
}
QLineEdit#txtTagFilter:hover, QLineEdit#txtPidFilter:hover { border-color: %15; }
QLineEdit#txtTagFilter:focus, QLineEdit#txtPidFilter:focus { border-color: %7; }

/* ─── Configuration tab — dumpsys panel ─── */
QWidget#dumpsysPanel { background-color: %11; }
QWidget#dumpsysControlsContainer {
    background-color: %16;
    border: 1px solid %12;
    border-radius: 8px;
}
QLabel#lblDumpsys {
    color: %13;
    font-weight: 700;
    font-size: 12px;
    letter-spacing: 0.6px;
    text-transform: uppercase;
}
QLabel#lblDumpsysService, QLabel#lblDumpsysFind, QLabel#lblDumpsysCommand {
    color: %17;
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 0.4px;
    text-transform: uppercase;
    padding: 2px 4px 2px 0;
}

QLineEdit#txtDumpsysService, QLineEdit#txtDumpsysSearch, QLineEdit#txtDumpsysCommand {
    background-color: %11;
    color: %13;
    border: 1px solid %12;
    border-radius: 6px;
    padding: 6px 10px;
    selection-background-color: %20;
    selection-color: %21;
    font-family: "JetBrains Mono","Fira Code","Cascadia Code","Menlo",monospace;
    font-size: 12px;
}
QLineEdit#txtDumpsysService:hover, QLineEdit#txtDumpsysSearch:hover,
QLineEdit#txtDumpsysCommand:hover { border-color: %15; }
QLineEdit#txtDumpsysService:focus, QLineEdit#txtDumpsysSearch:focus,
QLineEdit#txtDumpsysCommand:focus { border-color: %7; }

QPlainTextEdit#txtDumpsysCmdResult, QPlainTextEdit#txtDumpsysResult {
    background-color: %11;
    color: %13;
    border: 1px solid %12;
    border-radius: 8px;
    padding: 8px;
    font-family: "JetBrains Mono","Fira Code","Cascadia Code","Menlo",monospace;
    font-size: 12px;
    selection-background-color: %20;
    selection-color: %21;
}
QPlainTextEdit#txtDumpsysCmdResult:focus,
QPlainTextEdit#txtDumpsysResult:focus { border-color: %7; }

QSplitter#splitterDumpsysOutput::handle {
    background-color: %12;
}
QSplitter#splitterDumpsysOutput::handle:vertical { height: 4px; }
QSplitter#splitterDumpsysOutput::handle:horizontal { width: 4px; }
QSplitter#splitterDumpsysOutput::handle:hover { background-color: %7; }

/* ─── Cradle tab — modern panel layout ─── */
QGroupBox#grpCradleInfo, QGroupBox#grpCradleFirmware,
QGroupBox#grpCradleSchedule, QGroupBox#grpCradleOutput {
    background-color: %16;
    border: 1px solid %12;
    border-radius: 8px;
    margin-top: 14px;
    padding-top: 12px;
    font-weight: 700;
    font-size: 11px;
    color: %13;
    letter-spacing: 0.5px;
}
QGroupBox#grpCradleInfo::title, QGroupBox#grpCradleFirmware::title,
QGroupBox#grpCradleSchedule::title, QGroupBox#grpCradleOutput::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px; top: -2px;
    padding: 0 6px;
    background-color: %16;
    color: %13;
    text-transform: uppercase;
}

QLabel#lblCradleKey, QLabel#lblCradleFwType, QLabel#lblCradleDays,
QLabel#lblCradleCmd, QLabel#lblCradleLastCmd {
    color: %17;
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 0.4px;
    background: transparent;
}

QLineEdit#txtCradleKey, QLineEdit#txtCradleFwPath {
    background-color: %11;
    color: %13;
    border: 1px solid %12;
    border-radius: 6px;
    padding: 6px 10px;
    selection-background-color: %20;
    selection-color: %21;
    font-family: "JetBrains Mono","Fira Code","Cascadia Code","Menlo",monospace;
    font-size: 12px;
}
QLineEdit#txtCradleKey:hover, QLineEdit#txtCradleFwPath:hover { border-color: %15; }
QLineEdit#txtCradleKey:focus, QLineEdit#txtCradleFwPath:focus { border-color: %7; }

/* Cradle action buttons */
QPushButton#btnCradleGet, QPushButton#btnCradleQueryFirmware,
QPushButton#btnCradleQuerySchedule, QPushButton#btnCradleClearOutput {
    background-color: %16;
    color: %13;
    border: 1px solid %12;
    border-radius: 6px;
    padding: 6px 14px;
    font-weight: 600;
}
QPushButton#btnCradleGet:hover, QPushButton#btnCradleQueryFirmware:hover,
QPushButton#btnCradleQuerySchedule:hover, QPushButton#btnCradleClearOutput:hover {
    background-color: %14; border-color: %7;
}
QPushButton#btnCradleGet:pressed, QPushButton#btnCradleQueryFirmware:pressed,
QPushButton#btnCradleQuerySchedule:pressed, QPushButton#btnCradleClearOutput:pressed {
    background-color: %16;
}

/* Cradle update firmware — primary destructive-ish action */
QPushButton#btnCradleUpdateFirmware {
    background-color: %18; color: %8;
    border: none; border-radius: 6px;
    padding: 8px 16px; font-weight: 700;
    letter-spacing: 0.4px;
}
QPushButton#btnCradleUpdateFirmware:hover    { background-color: %19; }
QPushButton#btnCradleUpdateFirmware:pressed  { background-color: %19; }
QPushButton#btnCradleUpdateFirmware:disabled { background-color: %16; color: %17; border: 1px solid %12; }

/* Cradle output viewer */
QPlainTextEdit#txtCradleOutput {
    background-color: %11;
    color: %13;
    border: 1px solid %12;
    border-radius: 8px;
    padding: 8px;
    font-family: "JetBrains Mono","Fira Code","Cascadia Code","Menlo",monospace;
    font-size: 12px;
    selection-background-color: %20;
    selection-color: %21;
}
QPlainTextEdit#txtCradleOutput:focus { border-color: %7; }
QWidget#markLogHeader {
    background-color: %16;
    border-bottom: 1px solid %12;
}
QLabel#lblMarkLog {
    color: %17;
    font-weight: 700;
    font-size: 11px;
    padding: 6px 10px;
    text-transform: uppercase;
    letter-spacing: 0.6px;
}
QPushButton#btnClearAllMarked {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 2px;
}
QPushButton#btnClearAllMarked:hover  { background-color: %14; border-color: %12; }
QPushButton#btnClearAllMarked:pressed { background-color: %16; }

/* ═════════════════════════════════════════════════════════════════════════════
 * Devices tab — modern panel layout
 * ═════════════════════════════════════════════════════════════════════════════ */

/* Card containers (created at runtime in DevicesTabController::polishDashboardCards) */
QFrame[class="devCard"] {
    background-color: %16;
    border: 1px solid %12;
    border-radius: 10px;
}
QFrame#devStatsCard {
    background-color: %11;
    border: 1px solid %12;
}

#tabDevices       { background-color: %11; }
#devSidebar       { background-color: %16; border-right: 1px solid %12; }
#devRightPanel    { background-color: %11; }
QSplitter#devSplitter::handle { background-color: %12; width: 1px; }

#devLblAppName    { color: %7;  font-weight: 700; letter-spacing: 2px; }
#devLblAppVersion { color: %17; font-weight: 600; letter-spacing: 1.5px; font-size: 11px; }

QPushButton#devBtnAddGroup {
    background-color: transparent; color: %7;
    border: 1px dashed %7; border-radius: 6px; padding: 6px 12px;
    font-weight: 600;
}
QPushButton#devBtnAddGroup:hover { background-color: %20; color: %21; border-style: solid; }

/* Device-list scroll area */
QScrollArea#devListScrollArea { background: transparent; border: none; }
QScrollArea#devListScrollArea QScrollBar:vertical { width: 6px; background: transparent; }
QScrollArea#devListScrollArea QScrollBar::handle:vertical {
    background: %12; border-radius: 3px; min-height: 20px;
}
QScrollArea#devListScrollArea QScrollBar::handle:vertical:hover { background: %15; }
QScrollArea#devListScrollArea QScrollBar::add-line:vertical,
QScrollArea#devListScrollArea QScrollBar::sub-line:vertical { height: 0; }
#devListContents { background: transparent; }
#devNameLabel { color: %13; font-weight: 700; }

/* Inner tab bar (per-device) */
QTabWidget#devInnerTabWidget::pane { border: none; background: %11; }
QTabWidget#devInnerTabWidget QTabBar { background: %16; }
QTabWidget#devInnerTabWidget QTabBar::tab {
    background: %16; color: %17; border: none;
    border-bottom: 2px solid transparent;
    padding: 4px 26px; font-weight: 600; min-width: 110px; max-height: 25px;
}
QTabWidget#devInnerTabWidget QTabBar::tab:selected { color: %13; border-bottom: 2px solid %7; }
QTabWidget#devInnerTabWidget QTabBar::tab:hover:!selected { color: %13; background: %14; }

/* Dashboard scroll area */
QScrollArea#devDashScrollArea { background: %11; border: none; }
QScrollArea#devDashScrollArea QScrollBar:vertical { width: 8px; background: transparent; }
QScrollArea#devDashScrollArea QScrollBar::handle:vertical {
    background: %12; border-radius: 4px; min-height: 24px; margin: 2px;
}
QScrollArea#devDashScrollArea QScrollBar::handle:vertical:hover { background: %15; }
QScrollArea#devDashScrollArea QScrollBar::add-line:vertical,
QScrollArea#devDashScrollArea QScrollBar::sub-line:vertical { height: 0; }
#devDashContents { background-color: %11; }

/* Inline separators / titles */
#devInfoSep1, #devInfoSep2, #devInfoSep3 { color: %12; }
#devBatteryTitle, #devIpTitle, #devNetworkTitle,
#devWifiSsidTitle, #devWifiPassTitle {
    color: %17; letter-spacing: 1px; background: transparent; border: none;
    font-size: 11px; font-weight: 600; text-transform: uppercase;
}
#devBatteryValue, #devIpValue, #devNetworkValue {
    color: %13; font-weight: 700; background: transparent; border: none;
}
#devBatteryIcon, #devIpIcon, #devNetworkIcon,
#devWifiSsidIcon, #devWifiPassIcon {
    color: %17; background: transparent; border: none;
}
#devQaTitle, #devSiTitle, #devConfigTitle, #devDeviceListTitle,
#devFirmwareTitle, #devFirmwareIcon {
    color: %13; font-weight: 700; letter-spacing: 1.2px;
    background: transparent; border: none; text-transform: uppercase;
    font-size: 12px;
}

/* WiFi / Locale inputs */
#devWifiSsidEdit, #devWifiPassEdit, #devLocaleEdit, #devFirmwarePathEdit {
    background-color: %16; color: %13;
    border: 1px solid %12; border-radius: 6px; padding: 6px 10px;
}
#devWifiSsidEdit:hover, #devWifiPassEdit:hover,
#devLocaleEdit:hover, #devFirmwarePathEdit:hover { border-color: %15; }
#devWifiSsidEdit:focus, #devWifiPassEdit:focus,
#devLocaleEdit:focus, #devFirmwarePathEdit:focus { border-color: %7; }

/* System-info key/value rows */
#devSiManufacturerKey, #devSiModelKey, #devSiAndroidVersionKey,
#devSiSdkVersionKey, #devSiBuildNumberKey, #devSiBuildFingerprintKey,
#devSiSecurityPatchKey, #devSiKernelVersionKey, #devSiAbiKey {
    color: %17; letter-spacing: 0.8px; background: transparent;
    border: none; border-bottom: 1px solid %12; padding: 6px 0;
    font-size: 11px; font-weight: 600; text-transform: uppercase;
}
#devSiManufacturerValue, #devSiModelValue, #devSiAndroidVersionValue,
#devSiSdkVersionValue, #devSiBuildNumberValue, #devSiBuildFingerprintValue,
#devSiSecurityPatchValue, #devSiKernelVersionValue, #devSiAbiValue {
    color: %13; background: transparent; border: none;
    border-bottom: 1px solid %12; padding: 6px 0; font-weight: 500;
}

/* Inner config tab */
QTabWidget#devConfigTabWidget::pane { border: 1px solid %12; background: %16; border-radius: 6px; }
QTabWidget#devConfigTabWidget QTabBar { background: transparent; }
QTabWidget#devConfigTabWidget QTabBar::tab {
    background: transparent; color: %17; border: none;
    border-bottom: 2px solid transparent; padding: 3px 22px; font-weight: 600; min-width: 100px; max-height: 25px;
}
QTabWidget#devConfigTabWidget QTabBar::tab:selected { color: %13; border-bottom: 2px solid %7; }
QTabWidget#devConfigTabWidget QTabBar::tab:hover:!selected { color: %13; }

/* Primary action — deploy */
QPushButton#devBtnDeployConfig {
    background-color: %7; color: %8;
    border: none; border-radius: 6px; padding: 8px 18px;
    font-weight: 700; letter-spacing: 0.4px;
}
QPushButton#devBtnDeployConfig:hover   { background-color: %9; }
QPushButton#devBtnDeployConfig:pressed { background-color: %10; }
QPushButton#devBtnDeployConfig:disabled {
    background-color: %16; color: %17; border: 1px solid %12;
}

/* Ghost icon buttons (Export/Import/SavePreset/LoadPreset) */
QPushButton#devBtnExportJson, QPushButton#devBtnImportJson,
QPushButton#devBtnSavePreset, QPushButton#devBtnLoadPreset {
    background: transparent; border: 1px solid transparent;
    border-radius: 6px; padding: 6px;
}
QPushButton#devBtnExportJson:hover, QPushButton#devBtnImportJson:hover,
QPushButton#devBtnSavePreset:hover, QPushButton#devBtnLoadPreset:hover {
    background-color: %14; border-color: %12;
}

/* JSON viewer */
QTextEdit#devJsonView {
    background-color: %16; color: %13;
    border: 1px solid %12; border-radius: 6px;
    font-family: "JetBrains Mono","Fira Code","Cascadia Code","Menlo",monospace;
    font-size: 12px; padding: 6px;
    selection-background-color: %20; selection-color: %21;
}

/* Toggle / switch rows */
#devStayAwakeRow, #devAllowMockRow, #devVerifyAdbRow,
#devLocaleRow, #devTimeFormatRow {
    background: %16; border: 1px solid %12; border-radius: 8px;
}
#devStayAwakeIcon, #devAllowMockIcon, #devVerifyAdbIcon,
#devLocaleIcon, #devTimeFormatIcon {
    color: %13; background: transparent; border: none;
}
#devStayAwakeLabel, #devAllowMockLabel, #devVerifyAdbLabel,
#devLocaleLabel, #devTimeFormatLabel {
    color: %13; font-weight: 700; background: transparent; border: none;
}
#devStayAwakeDesc, #devAllowMockDesc, #devVerifyAdbDesc,
#devLocaleDesc, #devTimeFormatDesc {
    color: %17; background: transparent; border: none; font-size: 11px;
}

/* Browse-firmware secondary button */
QPushButton#devBtnBrowseFirmware {
    background-color: %16; color: %13;
    border: 1px solid %12; border-radius: 6px;
    padding: 6px 12px; font-weight: 600;
}
QPushButton#devBtnBrowseFirmware:hover  { background-color: %14; border-color: %7; }
QPushButton#devBtnBrowseFirmware:pressed { background-color: %16; }

/* Warning-class big buttons (flash / connect) — use danger token */
QPushButton#devBtnFlash, QPushButton#devBtnConnectWifi {
    background-color: %18; color: %8;
    border: none; border-radius: 6px; padding: 10px 14px;
    font-weight: 700; letter-spacing: 0.3px;
}
QPushButton#devBtnFlash:hover,   QPushButton#devBtnConnectWifi:hover   { background-color: %19; }
QPushButton#devBtnFlash:pressed, QPushButton#devBtnConnectWifi:pressed { background-color: %19; }
QPushButton#devBtnFlash:disabled, QPushButton#devBtnConnectWifi:disabled {
    background-color: %16; color: %17; border: 1px solid %12;
}

/* Quick-action grid buttons — accent ghost */
QPushButton#devBtnReboot, QPushButton#devBtnVolumeUp, QPushButton#devBtnRebootBootloader,
QPushButton#devBtnVolumeDown, QPushButton#devBtnRebootSideload, QPushButton#devBtnAdbWireless,
QPushButton#devBtnAdbRoot, QPushButton#devBtnAdbUnroot, QPushButton#devBtnRebootFastboot,
QPushButton#devBtnPowerKey {
    background-color: %20; color: %21;
    border: 1px solid %12; border-radius: 6px;
    padding: 10px 8px; font-weight: 600;
}
QPushButton#devBtnReboot:hover, QPushButton#devBtnVolumeUp:hover,
QPushButton#devBtnRebootBootloader:hover, QPushButton#devBtnVolumeDown:hover,
QPushButton#devBtnRebootSideload:hover, QPushButton#devBtnAdbWireless:hover,
QPushButton#devBtnAdbRoot:hover, QPushButton#devBtnAdbUnroot:hover,
QPushButton#devBtnRebootFastboot:hover, QPushButton#devBtnPowerKey:hover {
    background-color: %7; color: %8; border-color: %7;
}
QPushButton#devBtnReboot:pressed, QPushButton#devBtnVolumeUp:pressed,
QPushButton#devBtnRebootBootloader:pressed, QPushButton#devBtnVolumeDown:pressed,
QPushButton#devBtnRebootSideload:pressed, QPushButton#devBtnAdbWireless:pressed,
QPushButton#devBtnAdbRoot:pressed, QPushButton#devBtnAdbUnroot:pressed,
QPushButton#devBtnRebootFastboot:pressed, QPushButton#devBtnPowerKey:pressed {
    background-color: %10; color: %8; border-color: %10;
}
QPushButton#devBtnReboot:disabled, QPushButton#devBtnVolumeUp:disabled,
QPushButton#devBtnRebootBootloader:disabled, QPushButton#devBtnVolumeDown:disabled,
QPushButton#devBtnRebootSideload:disabled, QPushButton#devBtnAdbWireless:disabled,
QPushButton#devBtnAdbRoot:disabled, QPushButton#devBtnAdbUnroot:disabled,
QPushButton#devBtnRebootFastboot:disabled, QPushButton#devBtnPowerKey:disabled {
    background-color: %16; color: %17; border: 1px solid %12;
}
)").arg(
        /* %1  */ p.levelVerbose,
        /* %2  */ p.levelDebug,
        /* %3  */ p.levelInfo,
        /* %4  */ p.levelWarn,
        /* %5  */ p.levelError,
        /* %6  */ p.levelAssert,
        /* %7  */ p.accent,
        /* %8  */ p.textOnAccent,
        /* %9  */ p.accentHover)
       /* %10..%16 */
       .arg(p.accentActive, p.surface, p.border, p.text, p.surfaceHover,
            p.borderStrong, p.surfaceMuted)
       /* %17..%21 */
       .arg(p.textMuted, p.danger, p.dangerHover, p.accentSubtle,
            p.accentSubtleText)
       /* %22 */
       .arg(p.success);
}

QString lightStylesheet() { return fromPalette(Palette::light()); }
QString darkStylesheet()  { return fromPalette(Palette::dark()); }

} // namespace ThemeSheets
