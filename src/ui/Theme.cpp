#include "ui/Theme.h"

namespace ui {

const Theme& defaultTheme() {
    // Default-constructed Theme already holds the current values (see Theme.h).
    static const Theme theme;
    return theme;
}

} // namespace ui
