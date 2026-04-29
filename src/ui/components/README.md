# UiComponents — Design System Library

Centralized factory library for consistent UI widgets across ToolLogPro.
Every widget the app creates programmatically should come from this library so
that the look-and-feel stays uniform and a future redesign only requires
editing **one** stylesheet file (`src/ui/themesheets.cpp`) plus the factory
defaults here.

## Quick start

```cpp
#include "components/components.h"

using namespace UiComponents;

auto *save   = Button::make("Save", ButtonVariant::Primary, parent);
auto *cancel = Button::make("Cancel", ButtonVariant::Secondary, parent);
auto *del    = Button::make("Delete", ButtonVariant::Danger, parent);
auto *iconBtn = Button::icon(QIcon(":/icons/refresh.svg"), tr("Refresh"), parent);

auto *search = Input::search(tr("Search packages"), parent);
auto *name   = Input::make(tr("Property name"), parent);

auto *card = Card::make(tr("Filters"), parent);
auto *title = Label::h2(tr("Device Details"), parent);
```

## Components

### Button — `uibutton.h`

| Variant     | Use case                              |
|-------------|---------------------------------------|
| `Primary`   | Call-to-action (Save, Submit, Apply)  |
| `Secondary` | Default neutral button                |
| `Ghost`     | Transparent (toolbar inline actions)  |
| `Danger`    | Destructive (Delete, Clear, Remove)   |
| `Icon`      | Square icon-only                      |

Sizes: `Small` (24px), `Medium` (32px, default), `Large` (40px).

### Input — `uiinput.h`

| Variant    | Notes                                |
|------------|--------------------------------------|
| `Text`     | Default single-line input            |
| `Search`   | Includes built-in clear button       |
| `Password` | Echo-masked                          |

### Card — `uicard.h`

A `QGroupBox` wrapper that paints as a modern surface card.

| Helper      | Visual                                  |
|-------------|-----------------------------------------|
| `make()`    | Standard card (rounded, subtle border)  |
| `section()` | Stronger title, used for top-level groups |
| `flat()`    | No border — for nested grouping         |

### Label — `uilabel.h`

Typographic roles: `H1`, `H2`, `H3`, `Body`, `Caption`, `Mono`.

## How it works

Each factory:

1. Constructs the underlying Qt widget.
2. Sets `variant` / `size` / `role` **dynamic properties** on the widget.
3. Triggers `style()->unpolish/polish()` so QSS picks them up.

The QSS selectors in `themesheets.cpp` then key off these properties, e.g.:

```css
QPushButton[variant="primary"]      { background: #6366f1; color: white; }
QPushButton[variant="danger"]       { background: #dc2626; color: white; }
QLineEdit[variant="search"]         { padding-left: 28px; }
```

## Adding a new component

1. Create `src/ui/components/uiwidget.h` + `.cpp` following the pattern.
2. Add include to `components.h`.
3. Append the file pair to `CMakeLists.txt` under the components section.
4. Add the matching QSS selectors in `themesheets.cpp` (single template
   shared by all themes).

## Theming via design tokens

Colors are **never** hard-coded in `themesheets.cpp`. They come from a
`Palette` token struct (`palette.h`) with semantic fields like
`background`, `surface`, `accent`, `danger`, `levelError`, etc.

Two palettes ship today:

| File                  | Factory             |
|-----------------------|---------------------|
| `palette_light.cpp`   | `Palette::light()`  |
| `palette_dark.cpp`    | `Palette::dark()`   |

`themesheets.cpp::fromPalette(pal)` plugs tokens into a single QSS template
shared by both themes. The convenience accessors `lightStylesheet()` and
`darkStylesheet()` simply call `fromPalette()` with the right palette.

### Add a new theme

1. Create `src/ui/components/palette_<name>.cpp` returning a populated
   `Palette` from a `Palette::<name>()` factory you declare in
   `palette.h`.
2. Add the file pair to `CMakeLists.txt`.
3. Call `ThemeSheets::fromPalette(Palette::<name>())` from your runtime
   theme switcher.

No QSS edits are ever needed for a re-color — only token edits.

## Migration policy

- New code: **always** use `UiComponents::*` — never `new QPushButton(...)`
  inline.
- Legacy `mainwindow.ui`-driven widgets keep their styling; the global QSS
  still applies. Migrate them to dynamic properties on a per-tab basis when
  touching a tab's code.
- Inline `setStyleSheet()` is forbidden in new code — extend ThemeSheets
  instead.
