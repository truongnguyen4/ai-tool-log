#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("ToolLogPro"));
    a.setOrganizationName(QStringLiteral("ToolLogPro"));

    // Modern UI font: pick the first available from a curated stack.
    const QStringList preferredFamilies = {
        QStringLiteral("Inter"),
        QStringLiteral("SF Pro Display"),
        QStringLiteral("Segoe UI Variable"),
        QStringLiteral("Segoe UI"),
        QStringLiteral("Ubuntu"),
        QStringLiteral("Cantarell"),
        QStringLiteral("Noto Sans"),
        QStringLiteral("Roboto"),
    };
    const QStringList installed = QFontDatabase::families();
    QString chosen = QStringLiteral("Noto Sans");
    for (const QString &f : preferredFamilies) {
        if (installed.contains(f, Qt::CaseInsensitive)) { chosen = f; break; }
    }
    QFont uiFont(chosen, 10);
    uiFont.setHintingPreference(QFont::PreferFullHinting);
    uiFont.setStyleStrategy(QFont::PreferAntialias);
    a.setFont(uiFont);

    MainWindow w;
    w.show();
    return a.exec();
}

