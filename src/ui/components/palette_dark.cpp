#include "palette.h"

namespace UiComponents {

// ─────────────────────────────────────────────────────────────────────────────
// DARK PALETTE
// Near-black zinc surfaces + light indigo accent. High contrast, low glare.
// Edit only this file (and palette_light.cpp) to retheme the entire app.
// ─────────────────────────────────────────────────────────────────────────────
Palette Palette::dark()
{
    Palette p;

    // Surfaces
    p.background           = QStringLiteral("#0a0a0a");  // near-black
    p.surface              = QStringLiteral("#18181b");  // zinc-900
    p.surfaceMuted         = QStringLiteral("#0f0f10");  // tabs / panes bg
    p.surfaceHover         = QStringLiteral("#27272a");  // zinc-800

    // Borders
    p.border               = QStringLiteral("#27272a");  // zinc-800
    p.borderStrong         = QStringLiteral("#3f3f46");  // zinc-700
    p.divider              = QStringLiteral("#27272a");

    // Text
    p.text                 = QStringLiteral("#e4e4e7");  // zinc-200
    p.textMuted            = QStringLiteral("#a1a1aa");  // zinc-400
    p.textSubtle           = QStringLiteral("#71717a");  // zinc-500
    p.textOnAccent         = QStringLiteral("#ffffff");

    // Accent — indigo (modern, similar to Copilot UI)
    p.accent               = QStringLiteral("#818cf8");  // indigo-400
    p.accentHover          = QStringLiteral("#a5b4fc");  // indigo-300
    p.accentActive         = QStringLiteral("#6366f1");  // indigo-500
    p.accentSubtle         = QStringLiteral("#312e81");  // indigo-900
    p.accentSubtleText     = QStringLiteral("#c7d2fe");  // indigo-200
    p.focusRing            = QStringLiteral("#818cf8");

    // Semantic
    p.danger               = QStringLiteral("#f87171");  // red-400
    p.dangerHover          = QStringLiteral("#ef4444");  // red-500
    p.dangerSubtle         = QStringLiteral("#7f1d1d");  // red-900
    p.success              = QStringLiteral("#4ade80");  // green-400
    p.warning              = QStringLiteral("#fbbf24");  // amber-400
    p.info                 = QStringLiteral("#60a5fa");  // blue-400

    // Log levels (lighter for dark bg)
    p.levelVerbose         = QStringLiteral("#a1a1aa");
    p.levelDebug           = QStringLiteral("#60a5fa");
    p.levelInfo            = QStringLiteral("#4ade80");
    p.levelWarn            = QStringLiteral("#fbbf24");
    p.levelError           = QStringLiteral("#f87171");
    p.levelAssert          = QStringLiteral("#c084fc");  // purple-400

    // Scrollbars
    p.scrollbarHandle      = QStringLiteral("#3f3f46");
    p.scrollbarHandleHover = QStringLiteral("#52525b");
    p.scrollbarHandleActive = QStringLiteral("#71717a");

    // Tooltips (inverted = light)
    p.tooltipBg            = QStringLiteral("#fafafa");
    p.tooltipText          = QStringLiteral("#18181b");
    p.tooltipBorder        = QStringLiteral("#e4e4e7");

    return p;
}

} // namespace UiComponents
