#include "colorscheme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>

ColorScheme &ColorScheme::instance()
{
    static ColorScheme s;
    return s;
}

ColorScheme::ColorScheme(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    const QString stored = s.value(QStringLiteral("Theme/mode"),
                                   QStringLiteral("dark")).toString();
    if      (stored == QLatin1String("light")) m_mode = Mode::Light;
    else if (stored == QLatin1String("auto"))  m_mode = Mode::Auto;
    else                                       m_mode = Mode::Dark;
}

void ColorScheme::setMode(Mode m)
{
    if (m == m_mode) return;
    m_mode = m;

    QSettings s;
    const char *str = (m == Mode::Light) ? "light" : (m == Mode::Auto ? "auto" : "dark");
    s.setValue(QStringLiteral("Theme/mode"), QString::fromLatin1(str));

    emit modeChanged();
}

ColorScheme::Mode ColorScheme::resolvedMode() const
{
    if (m_mode != Mode::Auto)
        return m_mode;

    // Best-effort detection: ask Qt's current palette whether it looks dark.
    const QPalette pal = QApplication::palette();
    const QColor base  = pal.color(QPalette::Window);
    return base.lightness() < 128 ? Mode::Dark : Mode::Light;
}

QColor ColorScheme::levelColor(const QString &level) const
{
    const bool dark = resolvedMode() == Mode::Dark;
    if (level == QLatin1String("V")) return dark ? QColor("#9ca3af") : QColor("#4b5563"); // Verbose
    if (level == QLatin1String("D")) return dark ? QColor("#60a5fa") : QColor("#1d4ed8"); // Debug
    if (level == QLatin1String("I")) return dark ? QColor("#34d399") : QColor("#047857"); // Info
    if (level == QLatin1String("W")) return dark ? QColor("#fbbf24") : QColor("#b45309"); // Warn
    if (level == QLatin1String("E")) return dark ? QColor("#f87171") : QColor("#b91c1c"); // Error
    if (level == QLatin1String("A")) return dark ? QColor("#c084fc") : QColor("#7e22ce"); // Assert
    return dark ? QColor("#cccccc") : QColor("#1f2937");
}

QColor ColorScheme::markedRowBackground() const
{
    return resolvedMode() == Mode::Dark ? QColor("#1a3a80") : QColor("#bfdbfe");
}

QColor ColorScheme::anchorRowBackground() const
{
    return resolvedMode() == Mode::Dark ? QColor("#1a3a4a") : QColor("#bae6fd");
}

QColor ColorScheme::highlightBackground() const
{
    return resolvedMode() == Mode::Dark ? QColor("#b8860b") : QColor("#fde68a");
}

QColor ColorScheme::highlightForeground() const
{
    return resolvedMode() == Mode::Dark ? QColor("#ffffff") : QColor("#111827");
}

QColor ColorScheme::text() const
{
    return resolvedMode() == Mode::Dark ? QColor("#cccccc") : QColor("#1f1f1f");
}

QColor ColorScheme::mutedText() const
{
    return resolvedMode() == Mode::Dark ? QColor("#8a8a8a") : QColor("#6b6b6b");
}

QColor ColorScheme::accent() const
{
    return resolvedMode() == Mode::Dark ? QColor("#818cf8") : QColor("#14b8a6");
}

QColor ColorScheme::success() const
{
    return resolvedMode() == Mode::Dark ? QColor("#34d399") : QColor("#1a7f37");
}

QColor ColorScheme::border() const
{
    return resolvedMode() == Mode::Dark ? QColor("#3e3e42") : QColor("#d4d4d4");
}

QColor ColorScheme::rowSelectedBackground() const
{
    return resolvedMode() == Mode::Dark ? QColor("#4a4a52") : QColor("#dbeafe");
}

QColor ColorScheme::panelBackground() const
{
    return resolvedMode() == Mode::Dark ? QColor("#2d2d30") : QColor("#ffffff");
}

QColor ColorScheme::editorBackground() const
{
    return resolvedMode() == Mode::Dark ? QColor("#1e1e1e") : QColor("#ffffff");
}

QString ColorScheme::toHex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}
