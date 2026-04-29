#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QFont>
#include <QVector>
#include <QStringList>

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

    /** Returns the QSettings keys the user wants cleared in the Database tab. */
    QStringList   keysToReset()             const;

private:
    void setupUi();
    void setupFontTab();
    void setupColumnsTab();
    void setupPropDefColumnsTab();
    void setupDatabaseTab();
    void setupThemeTab();
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

    // Database tab — one checkbox per stored history group
    struct DbEntry { QString label; QString key; QCheckBox *cb = nullptr; };
    QVector<DbEntry> m_dbEntries;
};

#endif // SETTINGSDIALOG_H
