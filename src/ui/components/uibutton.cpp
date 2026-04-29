#include "uibutton.h"

#include <QStyle>

namespace UiComponents {

namespace {
const char *variantName(ButtonVariant v)
{
    switch (v) {
    case ButtonVariant::Primary:   return "primary";
    case ButtonVariant::Secondary: return "secondary";
    case ButtonVariant::Ghost:     return "ghost";
    case ButtonVariant::Danger:    return "danger";
    case ButtonVariant::Icon:      return "icon";
    }
    return "secondary";
}

const char *sizeName(ButtonSize s)
{
    switch (s) {
    case ButtonSize::Small:  return "sm";
    case ButtonSize::Medium: return "md";
    case ButtonSize::Large:  return "lg";
    }
    return "md";
}
} // namespace

QPushButton *Button::make(const QString &text,
                          ButtonVariant variant,
                          QWidget *parent,
                          ButtonSize size)
{
    auto *btn = new QPushButton(text, parent);
    style(btn, variant, size);
    return btn;
}

QPushButton *Button::icon(const QIcon &icon,
                          const QString &tooltip,
                          QWidget *parent,
                          ButtonSize size)
{
    auto *btn = new QPushButton(parent);
    btn->setIcon(icon);
    if (!tooltip.isEmpty()) btn->setToolTip(tooltip);
    style(btn, ButtonVariant::Icon, size);
    return btn;
}

void Button::style(QPushButton *btn, ButtonVariant variant, ButtonSize size)
{
    if (!btn) return;
    btn->setProperty("variant", variantName(variant));
    btn->setProperty("size", sizeName(size));
    btn->setCursor(Qt::PointingHandCursor);
    // Force re-polish so QSS picks up the new dynamic properties.
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
}

} // namespace UiComponents
