#ifndef UI_COMPONENTS_UILABEL_H
#define UI_COMPONENTS_UILABEL_H

#include <QLabel>
#include <QString>

namespace UiComponents {

/** Typographic role of a label — drives font-size/weight/color via QSS. */
enum class LabelRole {
    H1,       // 18pt, semibold
    H2,       // 15pt, semibold
    H3,       // 13pt, semibold
    Body,     // 11pt, normal (default)
    Caption,  // 10pt, normal, muted
    Mono      // monospace, code-ish
};

/**
 * Centralized factory for QLabel instances with consistent typography.
 */
class Label {
public:
    static QLabel *make(const QString &text,
                        LabelRole role = LabelRole::Body,
                        QWidget *parent = nullptr);

    static QLabel *h1(const QString &text, QWidget *parent = nullptr) {
        return make(text, LabelRole::H1, parent);
    }
    static QLabel *h2(const QString &text, QWidget *parent = nullptr) {
        return make(text, LabelRole::H2, parent);
    }
    static QLabel *h3(const QString &text, QWidget *parent = nullptr) {
        return make(text, LabelRole::H3, parent);
    }
    static QLabel *caption(const QString &text, QWidget *parent = nullptr) {
        return make(text, LabelRole::Caption, parent);
    }
    static QLabel *mono(const QString &text, QWidget *parent = nullptr) {
        return make(text, LabelRole::Mono, parent);
    }

    static void style(QLabel *label, LabelRole role);
};

} // namespace UiComponents

#endif // UI_COMPONENTS_UILABEL_H
