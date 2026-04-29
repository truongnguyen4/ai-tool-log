#include "settingsdialog.h"
#include "tableconfig.h"
#include "colorscheme.h"
#include "components/components.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QTabWidget>
#include <QFontComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QListWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QStackedWidget>
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
    setWindowTitle(tr("Settings"));
    setMinimumWidth(640);
    setMinimumHeight(440);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Build the tab widget invisibly so setup*Tab() methods can keep using
    // m_tabWidget->addTab(...) unchanged. We then migrate its pages to a
    // QListWidget sidebar + QStackedWidget content area.
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->hide();

    setupFontTab();
    setupColumnsTab();
    setupPropDefColumnsTab();
    setupDatabaseTab();
    setupThemeTab();

    // Build sidebar + stack from the tab widget's pages.
    auto *body = new QWidget(this);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto *sidebar = new QListWidget(body);
    sidebar->setObjectName(QStringLiteral("settingsSidebar"));
    sidebar->setFrameShape(QFrame::NoFrame);
    sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sidebar->setSelectionMode(QAbstractItemView::SingleSelection);
    sidebar->setUniformItemSizes(true);
    sidebar->setFixedWidth(180);

    auto *stack = new QStackedWidget(body);
    stack->setObjectName(QStringLiteral("settingsStack"));

    auto *stackHost = new QWidget(body);
    auto *hostLayout = new QVBoxLayout(stackHost);
    hostLayout->setContentsMargins(18, 18, 18, 18);
    hostLayout->setSpacing(0);
    hostLayout->addWidget(stack);

    while (m_tabWidget->count() > 0) {
        const QString title = m_tabWidget->tabText(0);
        QWidget *page = m_tabWidget->widget(0);
        m_tabWidget->removeTab(0);
        page->setParent(stack);
        stack->addWidget(page);
        auto *item = new QListWidgetItem(title, sidebar);
        item->setSizeHint(QSize(0, 44));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    m_tabWidget->deleteLater();
    m_tabWidget = nullptr;

    connect(sidebar, &QListWidget::currentRowChanged,
            stack,   &QStackedWidget::setCurrentIndex);
    sidebar->setCurrentRow(0);

    bodyLayout->addWidget(sidebar);
    bodyLayout->addWidget(stackHost, 1);

    mainLayout->addWidget(body, 1);

    // Footer with Apply / Cancel buttons.
    auto *footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("settingsFooter"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(18, 12, 18, 12);
    footerLayout->setSpacing(8);
    footerLayout->addStretch();

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, footer);
    if (auto *okBtn = buttonBox->button(QDialogButtonBox::Ok)) {
        UiComponents::Button::style(okBtn, UiComponents::ButtonVariant::Primary,
                                    UiComponents::ButtonSize::Medium);
        okBtn->setText(tr("Apply"));
    }
    if (auto *cancelBtn = buttonBox->button(QDialogButtonBox::Cancel)) {
        UiComponents::Button::style(cancelBtn, UiComponents::ButtonVariant::Secondary,
                                    UiComponents::ButtonSize::Medium);
    }
    footerLayout->addWidget(buttonBox);
    mainLayout->addWidget(footer);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Live font preview
    connect(m_fontComboBox, &QFontComboBox::currentFontChanged,
            this, [this]() { updatePreview(); });
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this]() { updatePreview(); });

    updatePreview();
    adjustSize();
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
    auto *btnSelectAll  = UiComponents::Button::make(tr("Select All"),
        UiComponents::ButtonVariant::Secondary, group, UiComponents::ButtonSize::Small);
    auto *btnSelectNone = UiComponents::Button::make(tr("Select None"),
        UiComponents::ButtonVariant::Ghost, group, UiComponents::ButtonSize::Small);
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

void SettingsDialog::setupThemeTab()
{
    auto *tab    = new QWidget;
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    auto *group  = new QGroupBox(tr("Application theme"), tab);
    auto *grpLay = new QVBoxLayout(group);
    grpLay->setSpacing(10);
    grpLay->setContentsMargins(14, 18, 14, 14);

    auto *rDark  = new QRadioButton(tr("Dark — modern slate palette"),  group);
    auto *rLight = new QRadioButton(tr("Light — clean Tailwind zinc + indigo"), group);
    auto *rAuto  = new QRadioButton(tr("Auto — follow system palette"), group);

    for (QRadioButton *r : { rDark, rLight, rAuto }) {
        r->setStyleSheet("padding: 6px 4px; font-weight: 500;");
    }

    switch (ColorScheme::instance().mode()) {
    case ColorScheme::Mode::Light: rLight->setChecked(true); break;
    case ColorScheme::Mode::Auto:  rAuto->setChecked(true);  break;
    case ColorScheme::Mode::Dark:
    default:                       rDark->setChecked(true);  break;
    }

    grpLay->addWidget(rDark);
    grpLay->addWidget(rLight);
    grpLay->addWidget(rAuto);

    auto applyChoice = [rDark, rLight, rAuto]() {
        if (rDark->isChecked())  ColorScheme::instance().setMode(ColorScheme::Mode::Dark);
        if (rLight->isChecked()) ColorScheme::instance().setMode(ColorScheme::Mode::Light);
        if (rAuto->isChecked())  ColorScheme::instance().setMode(ColorScheme::Mode::Auto);
    };
    connect(rDark,  &QRadioButton::toggled, this, applyChoice);
    connect(rLight, &QRadioButton::toggled, this, applyChoice);
    connect(rAuto,  &QRadioButton::toggled, this, applyChoice);

    layout->addWidget(group);

    auto *hint = new QLabel(
        tr("Theme changes apply instantly across the entire UI — log levels, "
           "tables, cards, buttons, and inputs all re-skin live."), tab);
    hint->setWordWrap(true);
    hint->setProperty("role", "caption");
    layout->addWidget(hint);
    layout->addStretch();

    m_tabWidget->addTab(tab, tr("Theme"));
}
