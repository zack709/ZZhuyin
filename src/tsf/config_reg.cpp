#include "tsf/config_reg.h"

#include <algorithm>

namespace zzhuyin::tsf {

namespace {

bool ReadDword(HKEY hKey, const wchar_t* name, DWORD& out_val) {
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    const LSTATUS st = RegQueryValueExW(
        hKey, name, nullptr, &type, reinterpret_cast<LPBYTE>(&out_val), &size);
    return (st == ERROR_SUCCESS && type == REG_DWORD);
}

bool WriteDword(HKEY hKey, const wchar_t* name, DWORD val) {
    return RegSetValueExW(
        hKey, name, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&val), sizeof(DWORD)) == ERROR_SUCCESS;
}

} // namespace

PersistedConfig ConfigStore::Load(const wchar_t* sub_key) {
    PersistedConfig cfg{};
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, sub_key, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return cfg;
    }

    DWORD v = 0;
    if (ReadDword(hKey, L"KeyboardLayout", v) && v >= 1 && v <= 4) {
        cfg.engine.layout = static_cast<zzhuyin::KeyboardLayout>(v);
    }
    if (ReadDword(hKey, L"OutputBig5Only", v)) {
        cfg.engine.charset_filter.output_big5_only = (v != 0);
    }
    if (ReadDword(hKey, L"IncludeExtA", v)) {
        cfg.engine.charset_filter.include_ext_a = (v != 0);
    }
    if (ReadDword(hKey, L"IncludeExtB", v)) {
        cfg.engine.charset_filter.include_ext_b = (v != 0);
    }
    if (ReadDword(hKey, L"AllowCns", v)) {
        cfg.engine.charset_filter.allow_cns = (v != 0);
    }
    if (ReadDword(hKey, L"IncludeHkscs", v)) {
        cfg.engine.charset_filter.include_hkscs = (v != 0);
    }
    if (ReadDword(hKey, L"PromptAssociatePhrase", v)) {
        cfg.engine.prompt_associate_phrase = (v != 0);
    }
    if (ReadDword(hKey, L"DefaultNativeMode", v)) {
        cfg.engine.default_native_mode = (v != 0);
    }
    if (ReadDword(hKey, L"EnablePunctuationInput", v)) {
        cfg.engine.enable_punctuation_input = (v != 0);
    }
    if (ReadDword(hKey, L"EnableWarningBeep", v)) {
        cfg.engine.enable_warning_beep = (v != 0);
    }
    if (ReadDword(hKey, L"LeftShiftToggleMode", v)) {
        cfg.engine.left_shift_toggle_mode = (v != 0);
    }
    if (ReadDword(hKey, L"RightShiftToggleMode", v)) {
        cfg.engine.right_shift_toggle_mode = (v != 0);
    }
    if (ReadDword(hKey, L"OutputSimplified", v)) {
        cfg.engine.output_simplified = (v != 0);
    }
    if (ReadDword(hKey, L"CandidateOrientation", v) && v <= 1) {
        cfg.engine.orientation = static_cast<zzhuyin::CandidateOrientation>(v);
    }
    if (ReadDword(hKey, L"FontSizePt", v)) {
        cfg.ui.font_size_pt = std::clamp(static_cast<int>(v), 9, 28);
    }
    if (ReadDword(hKey, L"DarkTheme", v)) {
        cfg.ui.dark_theme = (v != 0);
    }

    RegCloseKey(hKey);
    return cfg;
}

bool ConfigStore::Save(const PersistedConfig& cfg, const wchar_t* sub_key) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER, sub_key, 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    bool ok = true;
    ok &= WriteDword(hKey, L"KeyboardLayout", static_cast<DWORD>(cfg.engine.layout));
    ok &= WriteDword(hKey, L"OutputBig5Only", cfg.engine.charset_filter.output_big5_only ? 1u : 0u);
    ok &= WriteDword(hKey, L"IncludeExtA", cfg.engine.charset_filter.include_ext_a ? 1u : 0u);
    ok &= WriteDword(hKey, L"IncludeExtB", cfg.engine.charset_filter.include_ext_b ? 1u : 0u);
    ok &= WriteDword(hKey, L"AllowCns", cfg.engine.charset_filter.allow_cns ? 1u : 0u);
    ok &= WriteDword(hKey, L"IncludeHkscs", cfg.engine.charset_filter.include_hkscs ? 1u : 0u);
    ok &= WriteDword(hKey, L"PromptAssociatePhrase", cfg.engine.prompt_associate_phrase ? 1u : 0u);
    ok &= WriteDword(hKey, L"DefaultNativeMode", cfg.engine.default_native_mode ? 1u : 0u);
    ok &= WriteDword(hKey, L"EnablePunctuationInput", cfg.engine.enable_punctuation_input ? 1u : 0u);
    ok &= WriteDword(hKey, L"EnableWarningBeep", cfg.engine.enable_warning_beep ? 1u : 0u);
    ok &= WriteDword(hKey, L"LeftShiftToggleMode", cfg.engine.left_shift_toggle_mode ? 1u : 0u);
    ok &= WriteDword(hKey, L"RightShiftToggleMode", cfg.engine.right_shift_toggle_mode ? 1u : 0u);
    ok &= WriteDword(hKey, L"OutputSimplified", cfg.engine.output_simplified ? 1u : 0u);
    ok &= WriteDword(hKey, L"CandidateOrientation", static_cast<DWORD>(cfg.engine.orientation));
    ok &= WriteDword(hKey, L"FontSizePt", static_cast<DWORD>(std::clamp(cfg.ui.font_size_pt, 9, 28)));
    ok &= WriteDword(hKey, L"DarkTheme", cfg.ui.dark_theme ? 1u : 0u);

    RegCloseKey(hKey);
    return ok;
}

} // namespace zzhuyin::tsf
