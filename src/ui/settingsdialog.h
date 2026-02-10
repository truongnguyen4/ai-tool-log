#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QFont>
#include <QVector>

class QTabWidget;
class QFontComboBox;
class QSpinBox;
class QLabel;
class QCheckBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const QFont &currentFont,
                            const QVector<bool> &columnVisibility,
                            const QVector<bool> &propDefColumnVisibility,
                            QWidget *parent = nullptr);

    QFont         selectedFont()            const;
    QVector<bool> columnVisibility()        const;
    QVector<bool> propDefColumnVisibility() const;

private:
    void setupUi();
    void setupFontTab();
    void setupColumnsTab();
    void setupPropDefColumnsTab();
    void updatePreview();

    QTabWidget    *m_tabWidget       = nullptr;
    QFontComboBox *m_fontComboBox    = nullptr;
    QSpinBox      *m_fontSizeSpinBox = nullptr;
    QLabel        *m_previewLabel    = nullptr;

    QFont         m_currentFont;
    QVector<bool> m_initColumnVis;
    QVector<bool> m_initPropDefColVis;
    QVector<QCheckBox *> m_columnCheckboxes;
    QVector<QCheckBox *> m_propDefColumnCheckboxes;
};

#endif // SETTINGSDIALOG_H
