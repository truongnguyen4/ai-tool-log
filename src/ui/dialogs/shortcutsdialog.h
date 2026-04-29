#ifndef SHORTCUTSDIALOG_H
#define SHORTCUTSDIALOG_H

#include <QDialog>

class QWidget;

// Lightweight modal dialog showing all keyboard shortcuts. Triggered by F1.
class ShortcutsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ShortcutsDialog(QWidget *parent = nullptr);
};

#endif // SHORTCUTSDIALOG_H
