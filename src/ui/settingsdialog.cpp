#include "settingsdialog.h"
#include "tableconfig.h"

#include <QTabWidget>
#include <QFontComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QApplication>
#include <QPushButton>
#include <QMessageBox>

SettingsDialog::SettingsDialog(const QFont &currentFont,
                               const QVector<bool> &columnVisibility,
                               const QVector<bool> &propDefColumnVisibility,
                               QWidget *parent)
    : QDialog(parent)
    , m_currentFont(currentFont)
    , m_initColumnVis(columnVisibility)
    , m_initPropDefColVis(propDefColumnVisibility)
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(450, 380);
    setupUi();
}

QFont SettingsDialog::selectedFont() const
{
    QFont font = m_fontComboBox->currentFont();
    font.setPointSize(m_fontSizeSpinBox->value());
    return font;
}

QVector<bool> SettingsDialog::columnVisibility() const
{
    QVector<bool> vis;
    vis.reserve(m_columnCheckboxes.size());
    for (QCheckBox *cb : m_columnCheckboxes)
        vis.append(cb->isChecked());
    return vis;
}

QVector<bool> SettingsDialog::propDefColumnVisibility() const
{
    QVector<bool> vis;
    vis.reserve(m_propDefColumnCheckboxes.size());
    for (QCheckBox *cb : m_propDefColumnCheckboxes)
        vis.append(cb->isChecked());
    return vis;
}

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget(this);

    setupFontTab();
    setupColumnsTab();
    setupPropDefColumnsTab();
    setupDatabaseTab();

    mainLayout->addWidget(m_tabWidget);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Live font preview
    connect(m_fontComboBox, &QFontComboBox::currentFontChanged,
            this, [this]() { updatePreview(); });
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this]() { updatePreview(); });

    updatePreview();
}

void SettingsDialog::setupFontTab()
{
    auto *fontTab    = new QWidget;
    auto *fontLayout = new QVBoxLayout(fontTab);

    auto *fontGroup  = new QGroupBox(tr("Application Font"), fontTab);
    auto *formLayout = new QFormLayout(fontGroup);

    m_fontComboBox = new QFontComboBox(fontGroup);
    m_fontComboBox->setCurrentFont(m_currentFont);
    formLayout->addRow(tr("Family:"), m_fontComboBox);

    m_fontSizeSpinBox = new QSpinBox(fontGroup);
    m_fontSizeSpinBox->setRange(6, 48);
    m_fontSizeSpinBox->setValue(m_currentFont.pointSize() > 0
                                    ? m_currentFont.pointSize()
                                    : 10);
    m_fontSizeSpinBox->setSuffix(tr(" pt"));
    formLayout->addRow(tr("Size:"), m_fontSizeSpinBox);

    fontLayout->addWidget(fontGroup);

    auto *previewGroup  = new QGroupBox(tr("Preview"), fontTab);
    auto *previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(tr("The quick brown fox jumps over the lazy dog.\n"
                                   "0123456789  ABCDEFGHIJKLMNOPQRSTUVWXYZ"),
                                previewGroup);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumHeight(60);
    previewLayout->addWidget(m_previewLabel);
    fontLayout->addWidget(previewGroup);

    fontLayout->addStretch();

    m_tabWidget->addTab(fontTab, tr("Font"));
}

void SettingsDialog::setupColumnsTab()
{
    auto *colTab    = new QWidget;
    auto *colLayout = new QVBoxLayout(colTab);

    auto *colGroup  = new QGroupBox(tr("Visible Log Table Columns"), colTab);
    auto *grpLayout = new QVBoxLayout(colGroup);

    // Column names are sourced from TableConfig::LogColumns::Names to ensure consistency
    using namespace TableConfig::LogColumns;
    const QStringList colNames = {
        tr(Names::DATE),    // 0
        tr(Names::TIME),    // 1
        tr(Names::PID),     // 2
        tr(Names::TID),     // 3
        tr(Names::PACKAGE), // 4
        tr(Names::LEVEL),   // 5
        tr(Names::TAG),     // 6
        tr(Names::MESSAGE), // 7
    };

    m_columnCheckboxes.clear();
    for (int i = 0; i < colNames.size(); ++i) {
        auto *cb = new QCheckBox(colNames[i], colGroup);
        // Use supplied initial state; fall back to all-visible if list is short
        const bool visible = (i < m_initColumnVis.size()) ? m_initColumnVis[i] : true;
        cb->setChecked(visible);
        grpLayout->addWidget(cb);
        m_columnCheckboxes.append(cb);
    }

    // Note: MESSAGE cannot be hidden (table would look broken); enforce it
    if (!m_columnCheckboxes.isEmpty()) {
        m_columnCheckboxes.last()->setEnabled(false); // Message always visible
        m_columnCheckboxes.last()->setChecked(true);
    }

    colLayout->addWidget(colGroup);
    colLayout->addStretch();

    m_tabWidget->addTab(colTab, tr("Log Table Columns"));
}

void SettingsDialog::setupPropDefColumnsTab()
{
    auto *tab    = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    auto *group     = new QGroupBox(tr("Visible Property Definition Columns"), tab);
    auto *grpLayout = new QVBoxLayout(group);

    // Column names are sourced from TableConfig::PropertyDefColumns::Names to ensure consistency
    using namespace TableConfig::PropertyDefColumns;
    const QStringList colNames = {
        tr(Names::ID),            // 0
        tr(Names::NAME),          // 1
        tr(Names::SUPPORTED),     // 2
        tr(Names::NEED_REBOOT),   // 3
        tr(Names::TYPE),          // 4
        tr(Names::READ_ONLY),     // 5
        tr(Names::DEFAULT),       // 6
        tr(Names::VALUE),         // 7
        tr(Names::SET_BUTTON),    // 8
        tr(Names::GET_BUTTON),    // 9
        tr(Names::REMOVE_BUTTON), // 10
    };

    m_propDefColumnCheckboxes.clear();
    for (int i = 0; i < colNames.size(); ++i) {
        auto *cb = new QCheckBox(colNames[i], group);
        const bool visible = (i < m_initPropDefColVis.size()) ? m_initPropDefColVis[i] : true;
        cb->setChecked(visible);
        grpLayout->addWidget(cb);
        m_propDefColumnCheckboxes.append(cb);
    }

    layout->addWidget(group);
    layout->addStretch();

    m_tabWidget->addTab(tab, tr("Property Definition Columns"));
}

void SettingsDialog::updatePreview()
{
    m_previewLabel->setFont(selectedFont());
}

QStringList SettingsDialog::keysToReset() const
{
    QStringList keys;
    for (const DbEntry &e : m_dbEntries)
        if (e.cb && e.cb->isChecked())
            keys.append(e.key);
    return keys;
}

void SettingsDialog::setupDatabaseTab()
{
    auto *tab    = new QWidget;
    auto *layout = new QVBoxLayout(tab);

    auto *group     = new QGroupBox(tr("Filter History to Reset"), tab);
    auto *grpLayout = new QVBoxLayout(group);

    // Define which history groups exist and their friendly names
    m_dbEntries = {
        { tr("Logcat — Keyword filter"),          QStringLiteral("keyword")        },
        { tr("Logcat — Tag filter"),              QStringLiteral("tag")            },
        { tr("Logcat — PID filter"),              QStringLiteral("pid")            },
        { tr("Logcat — Package filter"),          QStringLiteral("package")        },
        { tr("Logcat — Find in message"),         QStringLiteral("findMessage")    },
        { tr("Settings table — Key filter"),      QStringLiteral("settingsKey")    },
        { tr("Settings table — Value filter"),    QStringLiteral("settingsValue")  },
        { tr("System properties — Key filter"),   QStringLiteral("propertiesKey")  },
        { tr("System properties — Value filter"), QStringLiteral("propertiesValue")},
    };

    for (DbEntry &e : m_dbEntries) {
        e.cb = new QCheckBox(e.label, group);
        grpLayout->addWidget(e.cb);
    }

    auto *btnRow        = new QHBoxLayout;
    auto *btnSelectAll  = new QPushButton(tr("Select All"),  group);
    auto *btnSelectNone = new QPushButton(tr("Select None"), group);
    btnRow->addWidget(btnSelectAll);
    btnRow->addWidget(btnSelectNone);
    btnRow->addStretch();

    connect(btnSelectAll,  &QPushButton::clicked, this, [this]() {
        for (DbEntry &e : m_dbEntries) if (e.cb) e.cb->setChecked(true);
    });
    connect(btnSelectNone, &QPushButton::clicked, this, [this]() {
        for (DbEntry &e : m_dbEntries) if (e.cb) e.cb->setChecked(false);
    });

    grpLayout->addLayout(btnRow);
    layout->addWidget(group);
    layout->addWidget(new QLabel(
        tr("Checked items will be cleared when you click OK."), tab));
    layout->addStretch();

    m_tabWidget->addTab(tab, tr("Database"));
}
