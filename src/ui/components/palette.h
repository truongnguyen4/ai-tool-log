#ifndef UI_COMPONENTS_PALETTE_H
#define UI_COMPONENTS_PALETTE_H

#include <QString>

namespace UiComponents {

/**
 * Design-token palette for a theme.
 *
 * Each token is a CSS color string ("#rrggbb" or "rgba(...)") and represents
 * a *semantic* role rather than a raw color. Stylesheets reference tokens
 * (e.g. `pal.surface`, `pal.accent`) so swapping a theme only requires
 * swapping the palette — no QSS edits needed.
 *
 * Two concrete palettes live in:
 *   - palette_light.cpp  →  Palette::light()
 *   - palette_dark.cpp   →  Palette::dark()
 *
 * Add a new theme by creating `palette_<name>.cpp` that returns a populated
 * Palette and exposing it via a static accessor here.
 */
struct Palette {
    // ── Surfaces ────────────────────────────────────────────────────────────
    QString background;       // App / window background
    QString surface;          // Cards, inputs, menus
    QString surfaceMuted;     // Headers, disabled fields, alt-row
    QString surfaceHover;     // Hover state for surface

    // ── Borders ─────────────────────────────────────────────────────────────
    QString border;           // Default 1px border
    QString borderStrong;     // Hover/focus border (subtle)
    QString divider;          // Splitter handles, separators

    // ── Text ────────────────────────────────────────────────────────────────
    QString text;             // Primary body text
    QString textMuted;        // Secondary text (labels, tooltips meta)
    QString textSubtle;       // Captions, placeholders
    QString textOnAccent;     // Text on accent-filled surface

    // ── Accent (primary brand color) ────────────────────────────────────────
    QString accent;           // Primary accent fill
    QString accentHover;      // Accent hover
    QString accentActive;     // Accent pressed
    QString accentSubtle;     // Selection/hover bg using accent
    QString accentSubtleText; // Text on accentSubtle bg
    QString focusRing;        // Focus border / outline

    // ── Semantic colors ─────────────────────────────────────────────────────
    QString danger;
    QString dangerHover;
    QString dangerSubtle;
    QString success;
    QString warning;
    QString info;

    // ── Log levels (Verbose+/V/D/I/W/E/A) ───────────────────────────────────
    QString levelVerbose;     // Verbose+ / V
    QString levelDebug;       // D
    QString levelInfo;        // I
    QString levelWarn;        // W
    QString levelError;       // E
    QString levelAssert;      // A

    // ── Scrollbars ──────────────────────────────────────────────────────────
    QString scrollbarHandle;
    QString scrollbarHandleHover;
    QString scrollbarHandleActive;

    // ── Tooltips (inverted contrast) ────────────────────────────────────────
    QString tooltipBg;
    QString tooltipText;
    QString tooltipBorder;

    // ─────────────────────────────────────────────────────────────────────────
    // Factories
    // ─────────────────────────────────────────────────────────────────────────
    static Palette light();
    static Palette dark();
};

} // namespace UiComponents

#endif // UI_COMPONENTS_PALETTE_H
