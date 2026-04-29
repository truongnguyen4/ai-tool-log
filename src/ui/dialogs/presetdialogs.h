#ifndef PRESETDIALOGS_H
#define PRESETDIALOGS_H

#include <QString>

class PresetStore;
class QWidget;

/**
 * Shared modal dialogs for preset save/load UX.
 *
 * Both the property-definition and devices-tab Save/Load preset flows used to
 * have copy-pasted dialog code; these helpers centralize them.
 */
namespace PresetDialogs {

/**
 * Prompt the user for a new preset name.
 * @return trimmed name, or empty string if cancelled / blank.
 */
QString askPresetName(QWidget *parent,
                      const QString &title,
                      const QString &prompt);

/**
 * Show a list of existing preset names with a Delete button. Double-click or
 * OK accepts the selection; deletes go straight through @p store.
 *
 * @return chosen preset name, or empty string if cancelled / nothing selected.
 */
QString pickPresetWithDelete(QWidget *parent,
                             const QString &title,
                             const QString &label,
                             PresetStore &store);

} // namespace PresetDialogs

#endif // PRESETDIALOGS_H
