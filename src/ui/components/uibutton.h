#ifndef UI_COMPONENTS_UIBUTTON_H
#define UI_COMPONENTS_UIBUTTON_H

#include <QPushButton>
#include <QString>
#include <QIcon>

namespace UiComponents {

/**
 * Visual variant for a UiButton. Drives QSS via the `variant` dynamic
 * property; ThemeSheets defines the matching selectors.
 */
enum class ButtonVariant {
    Primary,    // Filled accent (call to action)
    Secondary,  // Outlined neutral (default)
    Ghost,      // Transparent, hover-only background
    Danger,     // Filled red
    Icon        // Square, icon-only, no border
};

/** Visual size — drives padding & min-height. */
enum class ButtonSize {
    Small,    // 24px tall, compact padding
    Medium,   // 32px tall (default)
    Large     // 40px tall
};

/**
 * Centralized factory for QPushButton instances with consistent styling.
 *
 * Usage:
 *   auto *btn = UiComponents::Button::make("Save",
 *                                          UiComponents::ButtonVariant::Primary,
 *                                          parent);
 *   auto *icon = UiComponents::Button::icon(QIcon(":/icons/save.svg"), parent);
 *
 * Re-style an existing button without recreating it:
 *   UiComponents::Button::style(existingBtn, ButtonVariant::Danger);
 */
class Button {
public:
    /** Create a new QPushButton with the given variant + size. */
    static QPushButton *make(const QString &text,
                             ButtonVariant variant = ButtonVariant::Secondary,
                             QWidget *parent = nullptr,
                             ButtonSize size = ButtonSize::Medium);

    /** Convenience: an icon-only button (square, transparent background). */
    static QPushButton *icon(const QIcon &icon,
                             const QString &tooltip = QString(),
                             QWidget *parent = nullptr,
                             ButtonSize size = ButtonSize::Medium);

    /** Apply a variant + size to an existing button. */
    static void style(QPushButton *btn,
                      ButtonVariant variant,
                      ButtonSize size = ButtonSize::Medium);
};

} // namespace UiComponents

#endif // UI_COMPONENTS_UIBUTTON_H
