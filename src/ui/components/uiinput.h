#ifndef UI_COMPONENTS_UIINPUT_H
#define UI_COMPONENTS_UIINPUT_H

#include <QLineEdit>
#include <QString>

namespace UiComponents {

enum class InputVariant {
    Text,     // Default single-line input
    Search,   // Includes search affordances (clear button)
    Password  // Echo masked
};

enum class InputSize {
    Small,
    Medium,
    Large
};

/**
 * Centralized factory for QLineEdit instances with consistent styling.
 *
 * Usage:
 *   auto *e = UiComponents::Input::make("Enter property", parent);
 *   auto *s = UiComponents::Input::search("Search packages", parent);
 */
class Input {
public:
    static QLineEdit *make(const QString &placeholder = QString(),
                           QWidget *parent = nullptr,
                           InputVariant variant = InputVariant::Text,
                           InputSize size = InputSize::Medium);

    static QLineEdit *search(const QString &placeholder,
                             QWidget *parent = nullptr,
                             InputSize size = InputSize::Medium);

    static QLineEdit *password(const QString &placeholder = QString(),
                               QWidget *parent = nullptr,
                               InputSize size = InputSize::Medium);

    static void style(QLineEdit *edit,
                      InputVariant variant,
                      InputSize size = InputSize::Medium);
};

} // namespace UiComponents

#endif // UI_COMPONENTS_UIINPUT_H
