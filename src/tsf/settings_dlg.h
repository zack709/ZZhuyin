#pragma once

#include "tsf/config_reg.h"

#include <windows.h>
#include <functional>

namespace zzhuyin::tsf {

// Opens the 3-tab "Z注音 設定" window directly from zzhuyin_tsf.dll on its own UI thread
// (non-blocking for the host application). If already open, brings the existing window to front.
void ShowSettingsDialog(
    HINSTANCE hInst,
    std::function<void(const PersistedConfig&)> on_saved);

} // namespace zzhuyin::tsf
