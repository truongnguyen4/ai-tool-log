#ifndef THEMESHEETS_H
#define THEMESHEETS_H

#include <QString>

class QFont;
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

/**
 * Rules giving the log tables and read-only output views @p font. Append them
 * after a theme sheet: they override its font rules for those views only.
 * The tables' header labels keep @p interfaceFamily, the interface font.
 */
QString viewFontRules(const QFont &font, const QString &interfaceFamily);

} // namespace ThemeSheets

#endif // THEMESHEETS_H
