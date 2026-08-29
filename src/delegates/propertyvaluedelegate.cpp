#include "propertyvaluedelegate.h"
#include "propertydefinitionmodel.h"

#include <QComboBox>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QTimer>

#include <limits>

namespace {

const PropertyDefinition *definitionFor(const QModelIndex &index)
{
    const auto *model = qobject_cast<const PropertyDefinitionModel *>(index.model());
    return model ? model->definitionAt(index.row()) : nullptr;
}

bool fitsSpinBox(const PropertyDefinition &def)
{
    return def.hasRange && def.minimum >= std::numeric_limits<int>::min()
           && def.maximum <= std::numeric_limits<int>::max() && def.minimum <= def.maximum;
}

} // namespace

PropertyValueDelegate::PropertyValueDelegate(QObject *parent)
    : BlinkDelegate(parent)
{
}

QWidget *PropertyValueDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const
{
    const PropertyDefinition *def = definitionFor(index);
    if (!def)
        return BlinkDelegate::createEditor(parent, option, index);

    QWidget *editor = nullptr;
    switch (def->kind()) {
    case PropertyDefinition::Kind::Enum:
    case PropertyDefinition::Kind::MultiChoice:
        if (!def->allowedValues.isEmpty()) {
            auto *combo = new QComboBox(parent);
            for (qint64 value : def->allowedValues)
                combo->addItem(QString::number(value));
            // Picking a choice is the whole edit: commit it at once.
            connect(combo, &QComboBox::activated, this, [this, combo]() {
                emit const_cast<PropertyValueDelegate *>(this)->commitData(combo);
                emit const_cast<PropertyValueDelegate *>(this)->closeEditor(combo);
            });
            QTimer::singleShot(0, combo, &QComboBox::showPopup);
            editor = combo;
            break;
        }
        [[fallthrough]];
    case PropertyDefinition::Kind::Integer:
        if (fitsSpinBox(*def)) {
            auto *spin = new QSpinBox(parent);
            spin->setRange(int(def->minimum), int(def->maximum));
            // A table row is too short for usable step buttons; arrow keys
            // and the wheel still step, and the tooltip gives the range.
            spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
            spin->setAccelerated(true);
            editor = spin;
        } else {
            auto *line = new QLineEdit(parent);
            line->setValidator(new QRegularExpressionValidator(
                QRegularExpression(QStringLiteral("-?\\d*")), line));
            editor = line;
        }
        break;
    case PropertyDefinition::Kind::Char: {
        auto *line = new QLineEdit(parent);
        line->setMaxLength(1);
        editor = line;
        break;
    }
    case PropertyDefinition::Kind::Boolean:
    case PropertyDefinition::Kind::String:
    case PropertyDefinition::Kind::Blob:
    case PropertyDefinition::Kind::Other:
        editor = new QLineEdit(parent);
        break;
    }

    // Compact styling inside the row comes from the theme sheet.
    editor->setObjectName(QStringLiteral("propertyValueEditor"));
    editor->setToolTip(def->typeSummary());
    return editor;
}

void PropertyValueDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const QString text = index.data(Qt::EditRole).toString();
    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
        combo->setCurrentIndex(qMax(0, combo->findText(text)));
    } else if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
        spin->setValue(text.toInt());
    } else if (auto *line = qobject_cast<QLineEdit *>(editor)) {
        line->setText(text);
        line->selectAll();
    }
}

void PropertyValueDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                         const QModelIndex &index) const
{
    QString text;
    if (auto *combo = qobject_cast<QComboBox *>(editor))
        text = combo->currentText();
    else if (auto *spin = qobject_cast<QSpinBox *>(editor))
        text = QString::number(spin->value());
    else if (auto *line = qobject_cast<QLineEdit *>(editor))
        text = line->text();
    else
        return;

    auto *definitions = qobject_cast<PropertyDefinitionModel *>(model);
    const PropertyDefinition *def = definitionFor(index);
    if (!definitions || !def) {
        model->setData(index, text, Qt::EditRole);
        return;
    }
    const QString reason = definitions->stage(def->name, text);
    if (!reason.isEmpty())
        emit valueRejected(reason);
}

void PropertyValueDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                                 const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}
