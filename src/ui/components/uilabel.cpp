#include "uilabel.h"

#include <QStyle>

namespace UiComponents {

namespace {
const char *roleName(LabelRole r)
{
    switch (r) {
    case LabelRole::H1:      return "h1";
    case LabelRole::H2:      return "h2";
    case LabelRole::H3:      return "h3";
    case LabelRole::Body:    return "body";
    case LabelRole::Caption: return "caption";
    case LabelRole::Mono:    return "mono";
    }
    return "body";
}
} // namespace

QLabel *Label::make(const QString &text, LabelRole role, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    style(label, role);
    return label;
}

void Label::style(QLabel *label, LabelRole role)
{
    if (!label) return;
    label->setProperty("role", roleName(role));
    label->style()->unpolish(label);
    label->style()->polish(label);
}

} // namespace UiComponents
