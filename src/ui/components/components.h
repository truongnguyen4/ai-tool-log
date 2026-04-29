#ifndef UI_COMPONENTS_H
#define UI_COMPONENTS_H

/**
 * UiComponents — centralized UI widget factory library.
 *
 * Include this single header to access all template factories:
 *   UiComponents::Button
 *   UiComponents::Input
 *   UiComponents::Card
 *   UiComponents::Label
 *
 * Every factory returns a regular Qt widget (QPushButton, QLineEdit, ...)
 * with `variant` / `size` / `role` dynamic properties set so that the
 * QSS rules in ThemeSheets pick up the correct visual style.
 *
 * See src/ui/components/README.md for the full design system.
 */

#include "uibutton.h"
#include "uiinput.h"
#include "uicard.h"
#include "uilabel.h"

#endif // UI_COMPONENTS_H
