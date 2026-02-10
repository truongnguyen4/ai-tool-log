#include "mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setFont(QFont(QStringLiteral("Noto Sans"), 11));
    MainWindow w;
    w.show();
    return a.exec();
}
