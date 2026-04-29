#include "uiinput.h"

#include <QStyle>

namespace UiComponents {

namespace {
const char *variantName(InputVariant v)
{
    switch (v) {
    case InputVariant::Text:     return "text";
    case InputVariant::Search:   return "search";
    case InputVariant::Password: return "password";
    }
    return "text";
}

const char *sizeName(InputSize s)
{
    switch (s) {
    case InputSize::Small:  return "sm";
    case InputSize::Medium: return "md";
    case InputSize::Large:  return "lg";
    }
    return "md";
}
} // namespace

QLineEdit *Input::make(const QString &placeholder,
                       QWidget *parent,
                       InputVariant variant,
                       InputSize size)
{
    auto *edit = new QLineEdit(parent);
    if (!placeholder.isEmpty()) edit->setPlaceholderText(placeholder);
    style(edit, variant, size);
    return edit;
}

QLineEdit *Input::search(const QString &placeholder, QWidget *parent, InputSize size)
{
    auto *edit = make(placeholder, parent, InputVariant::Search, size);
    edit->setClearButtonEnabled(true);
    return edit;
}

QLineEdit *Input::password(const QString &placeholder, QWidget *parent, InputSize size)
{
    auto *edit = make(placeholder, parent, InputVariant::Password, size);
    edit->setEchoMode(QLineEdit::Password);
    return edit;
}

void Input::style(QLineEdit *edit, InputVariant variant, InputSize size)
{
    if (!edit) return;
    edit->setProperty("variant", variantName(variant));
    edit->setProperty("size", sizeName(size));
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
}

} // namespace UiComponents
