#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QFont>
#include <QVector>
#include <QStringList>

#include <optional>

class QTabWidget;
class QFontComboBox;
class QSpinBox;
class QLabel;
class QCheckBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @param viewFont        font of the log tables and output views; empty
     *                        while they use the theme default
     * @param defaultViewFont what those views show with no font chosen
     */
    explicit SettingsDialog(const std::optional<QFont> &viewFont,
                            const QFont &defaultViewFont,
                            const QVector<bool> &columnVisibility,
                            QWidget *parent = nullptr);

    /** The chosen log/output view font; empty to keep the theme default. */
    std::optional<QFont> viewFont()          const;
    QVector<bool> columnVisibility()        const;

    /** Returns the QSettings keys the user wants cleared in the Database tab. */
    QStringList   keysToReset()             const;

private:
    void setupUi();
    void setupFontTab();
    void setupColumnsTab();
    void setupDatabaseTab();
    void setupThemeTab();
    void updatePreview();
    /** The font described by the family and size controls. */
    QFont customFont() const;

    QTabWidget    *m_tabWidget       = nullptr;
    QCheckBox     *m_customFontCheck = nullptr;
    QFontComboBox *m_fontComboBox    = nullptr;
    QSpinBox      *m_fontSizeSpinBox = nullptr;
    QLabel        *m_previewLabel    = nullptr;

    std::optional<QFont> m_viewFont;
    QFont                m_defaultViewFont;
    QVector<bool> m_initColumnVis;
    QVector<QCheckBox *> m_columnCheckboxes;

    // Database tab — one checkbox per stored history group
    struct DbEntry { QString label; QString key; QCheckBox *cb = nullptr; };
    QVector<DbEntry> m_dbEntries;
};

#endif // SETTINGSDIALOG_H
