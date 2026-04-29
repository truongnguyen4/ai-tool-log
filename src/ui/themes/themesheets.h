#ifndef THEMESHEETS_H
#define THEMESHEETS_H

#include <QString>

namespace UiComponents { struct Palette; }

namespace ThemeSheets {

/**
 * Build a complete QSS sheet from a design-token Palette.
 *
 * The same QSS template drives both light and dark themes — only the
 * palette differs. Add a new theme by defining a new Palette factory in
 * src/ui/components/palette_<name>.cpp; no QSS changes needed.
 */
QString fromPalette(const UiComponents::Palette &pal);

/** Convenience: light theme stylesheet (uses Palette::light()). */
QString lightStylesheet();

/** Convenience: dark theme stylesheet (uses Palette::dark()). */
QString darkStylesheet();

} // namespace ThemeSheets

#endif // THEMESHEETS_H
