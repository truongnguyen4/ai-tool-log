// Shared one-shot styling helpers for QtWidgets used across controllers.
// Each helper bundles a stylesheet string + theme-color resolution that
// would otherwise be copy-pasted between unrelated setup() methods.
#pragma once

#include "colorscheme.h"

#include <QAbstractItemView>
#include <QCompleter>

namespace WidgetStyling {

// Apply the standard themed completer-popup stylesheet (background, text,
// border, selection colors from ColorScheme). Used by ConfigurationController
// and DumpsysController.
inline void styleCompleterPopup(QAbstractItemView *popup)
{
    if (!popup) return;
    const auto &cs = ColorScheme::instance();
    popup->setStyleSheet(QStringLiteral(
        "QListView {"
        "    background-color: %1;"
        "    color: %2;"
        "    border: 1px solid %3;"
        "    selection-background-color: %4;"
        "    selection-color: #ffffff;"
        "}").arg(
            ColorScheme::toHex(cs.panelBackground()),
            ColorScheme::toHex(cs.text()),
            ColorScheme::toHex(cs.border()),
            ColorScheme::toHex(cs.accent())));
}

} // namespace WidgetStyling
