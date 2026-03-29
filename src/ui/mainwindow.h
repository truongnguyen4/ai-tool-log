#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class UiManager;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ---------------------------------------------------------------------------
// MainWindow — thin shell.
//
// All logic, models, signal connections, and data live in UiManager.
// This class only:
//   • constructs the Ui form and UiManager
//   • delegates eventFilter() to UiManager::handleEvent()
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    UiManager      *m_uiManager;
};

#endif // MAINWINDOW_H
