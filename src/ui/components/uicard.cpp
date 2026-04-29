#include "uicard.h"

#include <QStyle>

namespace UiComponents {

QGroupBox *Card::make(const QString &title, QWidget *parent)
{
    auto *box = new QGroupBox(title, parent);
    style(box, "card");
    return box;
}

QGroupBox *Card::section(const QString &title, QWidget *parent)
{
    auto *box = new QGroupBox(title, parent);
    style(box, "section");
    return box;
}

QGroupBox *Card::flat(const QString &title, QWidget *parent)
{
    auto *box = new QGroupBox(title, parent);
    style(box, "flat");
    return box;
}

void Card::style(QGroupBox *box, const char *variant)
{
    if (!box) return;
    box->setProperty("variant", variant);
    box->style()->unpolish(box);
    box->style()->polish(box);
}

} // namespace UiComponents
