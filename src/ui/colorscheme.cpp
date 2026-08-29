#include "colorscheme.h"

#include "components/palette.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>

using UiComponents::Palette;

namespace {
constexpr auto kSettingsKey = "Theme/mode";

QString modeToString(ColorScheme::Mode m)
{
    switch (m) {
    case ColorScheme::Mode::Light: return QStringLiteral("light");
    case ColorScheme::Mode::Auto:  return QStringLiteral("auto");
    case ColorScheme::Mode::Dark:  break;
    }
    return QStringLiteral("dark");
}

ColorScheme::Mode modeFromString(const QString &s)
{
    if (s == QLatin1String("light")) return ColorScheme::Mode::Light;
    if (s == QLatin1String("auto"))  return ColorScheme::Mode::Auto;
    return ColorScheme::Mode::Dark;
}
} // namespace

// ---------------------------------------------------------------------------
// Resolved — every palette token pre-converted to QColor.
//
// levelColor() is called from LogModel::data() for every painted cell, so
// parsing "#rrggbb" on each call (as this class used to do) showed up in
// scroll profiles. Resolving once per theme switch removes that entirely.
// ---------------------------------------------------------------------------
struct ColorScheme::Resolved {
    ColorScheme::Mode mode;
    Palette palette;

    // Indexed by LogFilter::levelIndex(): 0=V 1=D 2=I 3=W 4=E 5=A.
    QColor level[6];
    QColor levelDefault;

    QColor markedRow;
    QColor anchorRow;
    QColor blink;
    QColor highlightBg;
    QColor highlightFg;

    QColor text;
    QColor mutedText;
    QColor accent;
    QColor success;
    QColor danger;
    QColor border;
    QColor rowSelected;
    QColor panel;
    QColor editor;

    explicit Resolved(ColorScheme::Mode m)
        : mode(m)
        , palette(m == ColorScheme::Mode::Light ? Palette::light() : Palette::dark())
    {
        level[0]     = QColor(palette.levelVerbose);
        level[1]     = QColor(palette.levelDebug);
        level[2]     = QColor(palette.levelInfo);
        level[3]     = QColor(palette.levelWarn);
        level[4]     = QColor(palette.levelError);
        level[5]     = QColor(palette.levelAssert);
        levelDefault = QColor(palette.levelDefault);

        markedRow   = QColor(palette.rowMarked);
        anchorRow   = QColor(palette.rowAnchor);
        blink       = QColor(palette.rowBlink);
        highlightBg = QColor(palette.searchHighlightBg);
        highlightFg = QColor(palette.searchHighlightText);

        text        = QColor(palette.text);
        mutedText   = QColor(palette.textMuted);
        accent      = QColor(palette.accent);
        success     = QColor(palette.success);
        danger      = QColor(palette.danger);
        border      = QColor(palette.border);
        rowSelected = QColor(palette.rowSelected);
        panel       = QColor(palette.panelBackground);
        editor      = QColor(palette.editorBackground);
    }
};

ColorScheme &ColorScheme::instance()
{
    static ColorScheme s;
    return s;
}

ColorScheme::ColorScheme(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    m_mode = modeFromString(s.value(QLatin1String(kSettingsKey),
                                    QStringLiteral("dark")).toString());
}

ColorScheme::~ColorScheme()
{
    delete m_cache;
}

void ColorScheme::setMode(Mode m)
{
    if (m == m_mode) return;
    m_mode = m;

    QSettings s;
    s.setValue(QLatin1String(kSettingsKey), modeToString(m));

    emit modeChanged();
}

ColorScheme::Mode ColorScheme::resolvedMode() const
{
    if (m_mode != Mode::Auto)
        return m_mode;

    // Best-effort detection: ask Qt's current palette whether it looks dark.
    const QColor window = QApplication::palette().color(QPalette::Window);
    return window.lightness() < 128 ? Mode::Dark : Mode::Light;
}

const ColorScheme::Resolved &ColorScheme::resolved() const
{
    const Mode m = resolvedMode();
    if (!m_cache || m_cache->mode != m) {
        delete m_cache;
        m_cache = new Resolved(m);
    }
    return *m_cache;
}

const Palette &ColorScheme::palette() const
{
    return resolved().palette;
}

QColor ColorScheme::levelColorByIndex(int levelIndex) const
{
    const Resolved &r = resolved();
    if (levelIndex < 0 || levelIndex >= 6)
        return r.levelDefault;
    return r.level[levelIndex];
}

QColor ColorScheme::levelColor(const QString &level) const
{
    if (level.isEmpty())
        return resolved().levelDefault;

    // Inline ordinal mapping — kept here (rather than calling LogFilter) so
    // that the UI layer does not depend on the filter layer.
    switch (level.at(0).toLatin1()) {
    case 'V': return levelColorByIndex(0);
    case 'D': return levelColorByIndex(1);
    case 'I': return levelColorByIndex(2);
    case 'W': return levelColorByIndex(3);
    case 'E': return levelColorByIndex(4);
    case 'A': return levelColorByIndex(5);
    default:  return resolved().levelDefault;
    }
}

QColor ColorScheme::markedRowBackground() const { return resolved().markedRow; }
QColor ColorScheme::anchorRowBackground() const { return resolved().anchorRow; }
QColor ColorScheme::blinkBackground()     const { return resolved().blink; }
QColor ColorScheme::highlightBackground() const { return resolved().highlightBg; }
QColor ColorScheme::highlightForeground() const { return resolved().highlightFg; }

QColor ColorScheme::text()                  const { return resolved().text; }
QColor ColorScheme::mutedText()             const { return resolved().mutedText; }
QColor ColorScheme::accent()                const { return resolved().accent; }
QColor ColorScheme::success()               const { return resolved().success; }
QColor ColorScheme::danger()                const { return resolved().danger; }
QColor ColorScheme::border()                const { return resolved().border; }
QColor ColorScheme::rowSelectedBackground() const { return resolved().rowSelected; }
QColor ColorScheme::panelBackground()       const { return resolved().panel; }
QColor ColorScheme::editorBackground()      const { return resolved().editor; }

QString ColorScheme::toHex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}
