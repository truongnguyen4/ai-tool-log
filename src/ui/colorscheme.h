#ifndef COLORSCHEME_H
#define COLORSCHEME_H

#include <QColor>
#include <QObject>
#include <QString>

namespace UiComponents { struct Palette; }

// ---------------------------------------------------------------------------
// ColorScheme — the single runtime source of truth for *widget-code* colours.
//
// It does not define any colour itself: every accessor resolves a semantic
// token from the active UiComponents::Palette (see palette_light.cpp /
// palette_dark.cpp), which is the same palette ThemeSheets uses to build the
// application stylesheet. Re-theming therefore means editing one palette file.
//
// Colours are resolved once per theme switch and cached, because accessors
// such as levelColor() are called from model data() on every painted cell.
// ---------------------------------------------------------------------------
class ColorScheme : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Dark, Light, Auto };

    static ColorScheme &instance();

    Mode mode() const { return m_mode; }
    void setMode(Mode m);

    // Resolved mode after honoring Auto (always Dark or Light).
    Mode resolvedMode() const;

    // The palette backing the currently resolved mode.
    const UiComponents::Palette &palette() const;

    // ---- Log-level foregrounds -------------------------------------------
    /** Foreground colour for a log-level letter ("V"/"D"/"I"/"W"/"E"/"A"). */
    QColor levelColor(const QString &level) const;
    /** Same, by pre-computed ordinal (see LogFilter::levelIndex); -1 = unknown. */
    QColor levelColorByIndex(int levelIndex) const;

    // ---- Data-view backgrounds -------------------------------------------
    QColor markedRowBackground() const;   ///< marked row in the log table
    QColor anchorRowBackground() const;   ///< ΔTime anchor row in the mark table
    QColor blinkBackground() const;       ///< "value just changed" flash
    QColor highlightBackground() const;   ///< dumpsys search match background
    QColor highlightForeground() const;   ///< dumpsys search match foreground

    // ---- General UI palette ----------------------------------------------
    QColor text() const;
    QColor mutedText() const;
    QColor accent() const;
    QColor success() const;
    QColor danger() const;
    QColor border() const;
    QColor rowSelectedBackground() const;
    QColor panelBackground() const;
    QColor editorBackground() const;

    /** Returns the colour as "#rrggbb" for use inside QSS string literals. */
    static QString toHex(const QColor &c);

signals:
    void modeChanged();

private:
    explicit ColorScheme(QObject *parent = nullptr);
    ~ColorScheme() override;

    // Resolved-colour cache. Keyed on the resolved mode so that Auto, which
    // follows the system palette, re-resolves without explicit invalidation.
    struct Resolved;
    const Resolved &resolved() const;

    Mode m_mode = Mode::Dark;
    mutable Resolved *m_cache = nullptr;   ///< owned
};

#endif // COLORSCHEME_H
