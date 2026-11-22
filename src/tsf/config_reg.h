#pragma once

#include "core/types.h"

#include <windows.h>

namespace zzhuyin::tsf {

struct UiConfig {
    int font_size_pt = 12;
};

struct PersistedConfig {
    zzhuyin::EngineConfig engine{};
    UiConfig ui{};
};

class ConfigStore {
public:
    static constexpr wchar_t kDefaultSubKey[] = L"Software\\ZZhuyin";

    static PersistedConfig Load(const wchar_t* sub_key = kDefaultSubKey);
    static bool Save(const PersistedConfig& cfg, const wchar_t* sub_key = kDefaultSubKey);
};

} // namespace zzhuyin::tsf
