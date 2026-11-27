#pragma once

#include "core/types.h"

#include <windows.h>
#include <string>

namespace zzhuyin::tsf {

struct UiConfig {
    int font_size_pt = 12;
    bool dark_theme = false;
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

    static std::string ToYaml(const PersistedConfig& cfg);
    static bool FromYaml(const std::string& yaml_text, PersistedConfig& out_cfg);

    static bool ExportYamlFile(const PersistedConfig& cfg, const wchar_t* file_path);
    static bool ImportYamlFile(const wchar_t* file_path, PersistedConfig& out_cfg);
};

} // namespace zzhuyin::tsf
