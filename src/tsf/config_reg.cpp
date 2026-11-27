#include "tsf/config_reg.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

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
    if (RegOpenKeyExW(HKEY_CURRENT_USER, sub_key, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) {
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
            REG_OPTION_NON_VOLATILE, KEY_WRITE | KEY_WOW64_64KEY, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
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

namespace {

std::string_view Trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

std::string ToLowerAscii(std::string_view s) {
    std::string out(s);
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::string_view StripQuotes(std::string_view s) {
    s = Trim(s);
    if (s.size() >= 2 &&
        ((s.front() == '"' && s.back() == '"') ||
         (s.front() == '\'' && s.back() == '\''))) {
        s.remove_prefix(1);
        s.remove_suffix(1);
    }
    return Trim(s);
}

bool ParseBool(std::string_view raw, bool& out) {
    const std::string v = ToLowerAscii(StripQuotes(raw));
    if (v == "true" || v == "1" || v == "yes" || v == "on") {
        out = true;
        return true;
    }
    if (v == "false" || v == "0" || v == "no" || v == "off") {
        out = false;
        return true;
    }
    return false;
}

bool ParseInt(std::string_view raw, int& out) {
    const std::string s(StripQuotes(raw));
    if (s.empty()) return false;
    try {
        size_t pos = 0;
        const int v = std::stoi(s, &pos);
        if (pos != s.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

const char* LayoutToYamlName(zzhuyin::KeyboardLayout layout) {
    switch (layout) {
        case zzhuyin::KeyboardLayout::Standard: return "standard";
        case zzhuyin::KeyboardLayout::Eten:     return "eten";
        case zzhuyin::KeyboardLayout::Ibm:      return "ibm";
        case zzhuyin::KeyboardLayout::GinYieh:  return "gin_yieh";
    }
    return "standard";
}

bool ParseLayout(std::string_view raw, zzhuyin::KeyboardLayout& out) {
    const std::string v = ToLowerAscii(StripQuotes(raw));
    if (v == "standard" || v == "1") {
        out = zzhuyin::KeyboardLayout::Standard;
        return true;
    }
    if (v == "eten" || v == "2") {
        out = zzhuyin::KeyboardLayout::Eten;
        return true;
    }
    if (v == "ibm" || v == "3") {
        out = zzhuyin::KeyboardLayout::Ibm;
        return true;
    }
    if (v == "gin_yieh" || v == "ginyieh" || v == "4") {
        out = zzhuyin::KeyboardLayout::GinYieh;
        return true;
    }
    return false;
}

const char* OrientationToYamlName(zzhuyin::CandidateOrientation o) {
    return (o == zzhuyin::CandidateOrientation::Horizontal) ? "horizontal" : "vertical";
}

bool ParseOrientation(std::string_view raw, zzhuyin::CandidateOrientation& out) {
    const std::string v = ToLowerAscii(StripQuotes(raw));
    if (v == "horizontal" || v == "0") {
        out = zzhuyin::CandidateOrientation::Horizontal;
        return true;
    }
    if (v == "vertical" || v == "1") {
        out = zzhuyin::CandidateOrientation::Vertical;
        return true;
    }
    return false;
}

} // namespace

std::string ConfigStore::ToYaml(const PersistedConfig& cfg) {
    std::ostringstream oss;
    oss << "# ZZhuyin (Z注音) Settings\n"
        << "keyboard_layout: " << LayoutToYamlName(cfg.engine.layout) << "\n"
        << "default_native_mode: " << (cfg.engine.default_native_mode ? "true" : "false") << "\n"
        << "candidate_orientation: " << OrientationToYamlName(cfg.engine.orientation) << "\n"
        << "font_size_pt: " << std::clamp(cfg.ui.font_size_pt, 9, 28) << "\n"
        << "dark_theme: " << (cfg.ui.dark_theme ? "true" : "false") << "\n"
        << "prompt_associate_phrase: " << (cfg.engine.prompt_associate_phrase ? "true" : "false") << "\n"
        << "output_simplified: " << (cfg.engine.output_simplified ? "true" : "false") << "\n"
        << "enable_punctuation_input: " << (cfg.engine.enable_punctuation_input ? "true" : "false") << "\n"
        << "enable_warning_beep: " << (cfg.engine.enable_warning_beep ? "true" : "false") << "\n"
        << "left_shift_toggle_mode: " << (cfg.engine.left_shift_toggle_mode ? "true" : "false") << "\n"
        << "right_shift_toggle_mode: " << (cfg.engine.right_shift_toggle_mode ? "true" : "false") << "\n"
        << "charset_filter:\n"
        << "  output_big5_only: " << (cfg.engine.charset_filter.output_big5_only ? "true" : "false") << "\n"
        << "  include_ext_a: " << (cfg.engine.charset_filter.include_ext_a ? "true" : "false") << "\n"
        << "  include_ext_b: " << (cfg.engine.charset_filter.include_ext_b ? "true" : "false") << "\n"
        << "  allow_cns: " << (cfg.engine.charset_filter.allow_cns ? "true" : "false") << "\n"
        << "  include_hkscs: " << (cfg.engine.charset_filter.include_hkscs ? "true" : "false") << "\n";
    return oss.str();
}

bool ConfigStore::FromYaml(const std::string& yaml_text, PersistedConfig& out_cfg) {
    PersistedConfig parsed{};
    std::string_view text(yaml_text);
    // Strip UTF-8 BOM if present
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }

    int recognized_keys = 0;
    std::string current_section;

    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        start = (end == text.size()) ? text.size() + 1 : end + 1;

        // Strip '#' comments (not inside quotes)
        bool in_single = false;
        bool in_double = false;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '\'' && !in_double) in_single = !in_single;
            else if (line[i] == '"' && !in_single) in_double = !in_double;
            else if (line[i] == '#' && !in_single && !in_double) {
                line = line.substr(0, i);
                break;
            }
        }

        const size_t indent = line.find_first_not_of(" \t");
        if (indent == std::string_view::npos) {
            continue;
        }
        std::string_view trimmed = Trim(line);
        if (trimmed.empty()) continue;

        const size_t colon = trimmed.find(':');
        if (colon == std::string_view::npos) continue;

        const std::string key = ToLowerAscii(Trim(trimmed.substr(0, colon)));
        const std::string_view val = Trim(trimmed.substr(colon + 1));

        if (indent == 0) {
            if (val.empty()) {
                current_section = key;
                continue;
            }
            current_section.clear();
        }

        const std::string full_key =
            (indent > 0 && !current_section.empty()) ? (current_section + "." + key) : key;

        bool bval = false;
        int ival = 0;

        if (full_key == "keyboard_layout") {
            if (ParseLayout(val, parsed.engine.layout)) ++recognized_keys;
        } else if (full_key == "default_native_mode") {
            if (ParseBool(val, bval)) { parsed.engine.default_native_mode = bval; ++recognized_keys; }
        } else if (full_key == "candidate_orientation") {
            if (ParseOrientation(val, parsed.engine.orientation)) ++recognized_keys;
        } else if (full_key == "font_size_pt") {
            if (ParseInt(val, ival)) { parsed.ui.font_size_pt = std::clamp(ival, 9, 28); ++recognized_keys; }
        } else if (full_key == "dark_theme") {
            if (ParseBool(val, bval)) { parsed.ui.dark_theme = bval; ++recognized_keys; }
        } else if (full_key == "prompt_associate_phrase") {
            if (ParseBool(val, bval)) { parsed.engine.prompt_associate_phrase = bval; ++recognized_keys; }
        } else if (full_key == "output_simplified") {
            if (ParseBool(val, bval)) { parsed.engine.output_simplified = bval; ++recognized_keys; }
        } else if (full_key == "enable_punctuation_input") {
            if (ParseBool(val, bval)) { parsed.engine.enable_punctuation_input = bval; ++recognized_keys; }
        } else if (full_key == "enable_warning_beep") {
            if (ParseBool(val, bval)) { parsed.engine.enable_warning_beep = bval; ++recognized_keys; }
        } else if (full_key == "left_shift_toggle_mode") {
            if (ParseBool(val, bval)) { parsed.engine.left_shift_toggle_mode = bval; ++recognized_keys; }
        } else if (full_key == "right_shift_toggle_mode") {
            if (ParseBool(val, bval)) { parsed.engine.right_shift_toggle_mode = bval; ++recognized_keys; }
        } else if (full_key == "charset_filter.output_big5_only" || full_key == "output_big5_only") {
            if (ParseBool(val, bval)) { parsed.engine.charset_filter.output_big5_only = bval; ++recognized_keys; }
        } else if (full_key == "charset_filter.include_ext_a" || full_key == "include_ext_a") {
            if (ParseBool(val, bval)) { parsed.engine.charset_filter.include_ext_a = bval; ++recognized_keys; }
        } else if (full_key == "charset_filter.include_ext_b" || full_key == "include_ext_b") {
            if (ParseBool(val, bval)) { parsed.engine.charset_filter.include_ext_b = bval; ++recognized_keys; }
        } else if (full_key == "charset_filter.allow_cns" || full_key == "allow_cns") {
            if (ParseBool(val, bval)) { parsed.engine.charset_filter.allow_cns = bval; ++recognized_keys; }
        } else if (full_key == "charset_filter.include_hkscs" || full_key == "include_hkscs") {
            if (ParseBool(val, bval)) { parsed.engine.charset_filter.include_hkscs = bval; ++recognized_keys; }
        }
    }

    if (recognized_keys == 0) {
        return false;
    }
    out_cfg = parsed;
    return true;
}

bool ConfigStore::ExportYamlFile(const PersistedConfig& cfg, const wchar_t* file_path) {
    if (!file_path || !*file_path) return false;
    std::ofstream ofs(file_path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    const std::string yaml = ToYaml(cfg);
    ofs.write(yaml.data(), static_cast<std::streamsize>(yaml.size()));
    return ofs.good();
}

bool ConfigStore::ImportYamlFile(const wchar_t* file_path, PersistedConfig& out_cfg) {
    if (!file_path || !*file_path) return false;
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs) return false;
    std::ostringstream oss;
    oss << ifs.rdbuf();
    if (!ifs.good() && !ifs.eof()) return false;
    return FromYaml(oss.str(), out_cfg);
}

} // namespace zzhuyin::tsf
