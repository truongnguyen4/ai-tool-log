#include "palette.h"

namespace UiComponents {

// ─────────────────────────────────────────────────────────────────────────────
// LIGHT PALETTE
// Inspired by Tailwind zinc + indigo. Clean, neutral, low chroma.
// Edit only this file (and palette_dark.cpp) to retheme the entire app.
// ─────────────────────────────────────────────────────────────────────────────
Palette Palette::light()
{
    Palette p;

    // Surfaces
    p.background           = QStringLiteral("#fafafa");  // zinc-50
    p.surface              = QStringLiteral("#ffffff");
    p.surfaceMuted         = QStringLiteral("#f4f4f5");  // zinc-100
    p.surfaceHover         = QStringLiteral("#f4f4f5");

    // Borders
    p.border               = QStringLiteral("#e4e4e7");  // zinc-200
    p.borderStrong         = QStringLiteral("#d4d4d8");  // zinc-300
    p.divider              = QStringLiteral("#e4e4e7");

    // Text
    p.text                 = QStringLiteral("#18181b");  // zinc-900
    p.textMuted            = QStringLiteral("#52525b");  // zinc-600
    p.textSubtle           = QStringLiteral("#71717a");  // zinc-500
    p.textOnAccent         = QStringLiteral("#ffffff");

    // Accent — indigo
    p.accent               = QStringLiteral("#14b8a6");  // teal-500
    p.accentHover          = QStringLiteral("#0d9488");  // teal-600
    p.accentActive         = QStringLiteral("#0f766e");  // teal-700
    p.accentSubtle         = QStringLiteral("#f0fdfa");  // teal-50
    p.accentSubtleText     = QStringLiteral("#0f766e");
    p.focusRing            = QStringLiteral("#14b8a6");

    // Semantic
    p.danger               = QStringLiteral("#dc2626");  // red-600
    p.dangerHover          = QStringLiteral("#b91c1c");  // red-700
    p.dangerSubtle         = QStringLiteral("#fee2e2");  // red-100
    p.success              = QStringLiteral("#16a34a");  // green-600
    p.warning              = QStringLiteral("#d97706");  // amber-600
    p.info                 = QStringLiteral("#2563eb");  // blue-600

    // Log levels
    p.levelVerbose         = QStringLiteral("#71717a");
    p.levelDebug           = QStringLiteral("#2563eb");
    p.levelInfo            = QStringLiteral("#16a34a");
    p.levelWarn            = QStringLiteral("#d97706");
    p.levelError           = QStringLiteral("#dc2626");
    p.levelAssert          = QStringLiteral("#9333ea");  // purple-600
    p.levelDefault         = QStringLiteral("#1f2937");

    // Data views
    p.rowMarked            = QStringLiteral("#bfdbfe");  // blue-200
    p.rowAnchor            = QStringLiteral("#bae6fd");  // sky-200
    p.rowSelected          = QStringLiteral("#dbeafe");  // blue-100
    p.rowBlink             = QStringLiteral("#dbeafe");  // blue-100 flash
    p.searchHighlightBg    = QStringLiteral("#fde68a");
    p.searchHighlightText  = QStringLiteral("#111827");
    p.panelBackground      = QStringLiteral("#ffffff");
    p.editorBackground     = QStringLiteral("#ffffff");

    // Scrollbars
    p.scrollbarHandle      = QStringLiteral("#d4d4d8");
    p.scrollbarHandleHover = QStringLiteral("#a1a1aa");
    p.scrollbarHandleActive = QStringLiteral("#71717a");

    // Tooltips (inverted)
    p.tooltipBg            = QStringLiteral("#18181b");
    p.tooltipText          = QStringLiteral("#fafafa");
    p.tooltipBorder        = QStringLiteral("#27272a");

    return p;
}

} // namespace UiComponents
