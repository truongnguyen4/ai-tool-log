#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "uimanager.h"
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_uiManager(new UiManager(ui, this))
{
    ui->setupUi(this);
    m_uiManager->initialize();
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (m_uiManager->handleEvent(obj, event))
        return true;
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_uiManager->persistFilterHistory();
    QMainWindow::closeEvent(event);
}
