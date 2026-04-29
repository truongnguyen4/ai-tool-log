#include "shortcutsdialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>

namespace {

struct Row {
    const char *keys;
    const char *action;
};

const Row kShortcuts[] = {
    // Application-wide
    { "F1",            "Show this Keyboard Shortcuts dialog" },
    { "Ctrl+C",        "Copy selected log rows / cells" },
    { "Ctrl+V",        "Paste into focused text input" },

    // Filter inputs
    { "Up / Down",     "Navigate filter history (when filter input is focused)" },
    { "Enter",         "Apply current filter / accept history entry" },

    // Tables
    { "Double-click",  "Show full cell content in detail panel" },
    { "Right-click",   "Open context menu (add to filter, copy, mark, …)" },

    // Dumpsys tab
    { "Enter",         "Run dumpsys / raw command" },
    { "Enter (search)","Find next match in dumpsys output" },
};

} // namespace

ShortcutsDialog::ShortcutsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(520, 420);

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        tr("These shortcuts are active throughout ToolLogPro. "
           "Filter-history shortcuts only apply while a filter input is focused."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    const int rowCount = int(sizeof(kShortcuts) / sizeof(kShortcuts[0]));
    auto *table = new QTableWidget(rowCount, 2, this);
    table->setHorizontalHeaderLabels({ tr("Shortcut"), tr("Action") });
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0, 160);
    table->setAlternatingRowColors(true);

    for (int i = 0; i < rowCount; ++i) {
        table->setItem(i, 0, new QTableWidgetItem(QString::fromUtf8(kShortcuts[i].keys)));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromUtf8(kShortcuts[i].action)));
    }
    layout->addWidget(table, 1);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(box);
}
