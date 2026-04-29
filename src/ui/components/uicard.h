#ifndef UI_COMPONENTS_UICARD_H
#define UI_COMPONENTS_UICARD_H

#include <QGroupBox>
#include <QString>

namespace UiComponents {

/**
 * Centralized factory for QGroupBox-based "card" containers with
 * consistent rounded-corner / subtle-border styling.
 *
 * Usage:
 *   auto *card = UiComponents::Card::make("Filters", parent);
 *   card->setLayout(myLayout);
 */
class Card {
public:
    /** Standard titled card (group box with rounded border). */
    static QGroupBox *make(const QString &title = QString(),
                           QWidget *parent = nullptr);

    /** A more prominent "section" card with stronger title. */
    static QGroupBox *section(const QString &title,
                              QWidget *parent = nullptr);

    /** Flat card — no border, used for grouping inside another card. */
    static QGroupBox *flat(const QString &title = QString(),
                           QWidget *parent = nullptr);

    static void style(QGroupBox *box, const char *variant);
};

} // namespace UiComponents

#endif // UI_COMPONENTS_UICARD_H
