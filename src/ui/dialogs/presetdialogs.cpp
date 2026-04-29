#include "presetdialogs.h"
#include "presetstore.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace PresetDialogs {

QString askPresetName(QWidget *parent, const QString &title, const QString &prompt)
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        parent, title, prompt, QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return {};
    return name;
}

QString pickPresetWithDelete(QWidget *parent,
                             const QString &title,
                             const QString &label,
                             PresetStore &store)
{
    const QStringList names = store.listPresets();
    if (names.isEmpty()) {
        QMessageBox::information(parent, title,
                                 QObject::tr("No saved presets found."));
        return {};
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumSize(360, 300);

    auto *layout     = new QVBoxLayout(&dialog);
    auto *labelW     = new QLabel(label, &dialog);
    auto *listWidget = new QListWidget(&dialog);
    listWidget->addItems(names);
    listWidget->setCurrentRow(0);

    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto *btnDelete = new QPushButton(QObject::tr("Delete"), &dialog);
    btnBox->addButton(btnDelete, QDialogButtonBox::ResetRole);

    layout->addWidget(labelW);
    layout->addWidget(listWidget);
    layout->addWidget(btnBox);

    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(btnDelete, &QPushButton::clicked, &dialog, [&]() {
        QListWidgetItem *item = listWidget->currentItem();
        if (!item) return;
        const QString n = item->text();
        const auto reply = QMessageBox::question(
            &dialog, QObject::tr("Delete Preset"),
            QObject::tr("Delete \"%1\"?").arg(n),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        QString err;
        if (store.deletePreset(n, err)) {
            delete listWidget->takeItem(listWidget->row(item));
        } else {
            QMessageBox::critical(&dialog, QObject::tr("Delete Failed"), err);
        }
    });
    QObject::connect(listWidget, &QListWidget::doubleClicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted)
        return {};
    QListWidgetItem *selected = listWidget->currentItem();
    return selected ? selected->text() : QString();
}

} // namespace PresetDialogs
