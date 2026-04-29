#ifndef COLORSCHEME_H
#define COLORSCHEME_H

#include <QColor>
#include <QObject>
#include <QString>

// Centralized colour palette. Replaces hardcoded #xxxxxx literals scattered
// across LogModel, MarkLogModel, and the main UI stylesheet. Accessed as
// a singleton; mode can be switched at runtime (currently only the log-level
// palette is mode-aware; mainwindow.ui stylesheet swap is pending).
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

    // Log-level foreground colour (V/D/I/W/E/A).
    QColor levelColor(const QString &level) const;

    // Marked-row background (used by LogModel BackgroundRole).
    QColor markedRowBackground() const;

    // Anchor-row background (used by MarkLogModel BackgroundRole).
    QColor anchorRowBackground() const;

    // Dumpsys search highlight (background / foreground).
    QColor highlightBackground() const;
    QColor highlightForeground() const;

    // ---- General UI palette (used by Devices tab inline stylesheets) -------
    QColor text() const;          // primary text
    QColor mutedText() const;     // secondary / muted text
    QColor accent() const;        // brand accent (#007acc family)
    QColor success() const;       // online/connected indicator
    QColor border() const;        // subtle separator/border
    QColor rowSelectedBackground() const; // "selected device row" tint
    QColor panelBackground() const;       // dropdown/menu/dialog panel bg
    QColor editorBackground() const;      // text edit / log panel bg

    /** Returns the colour as "#rrggbb" for use inside QSS string literals. */
    static QString toHex(const QColor &c);

signals:
    void modeChanged();

private:
    explicit ColorScheme(QObject *parent = nullptr);
    Mode m_mode = Mode::Dark;
};

#endif // COLORSCHEME_H
