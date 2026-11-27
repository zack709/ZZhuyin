#include "tsf/config_reg.h"
#include "tsf/guids.h"

#include <ctffunc.h>
#include <msctf.h>
#include <windows.h>

#include <iostream>

int main() {
    const HRESULT co_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(co_hr)) {
        std::cerr << "CoInitializeEx failed\n";
        return 1;
    }

    HMODULE mod = LoadLibraryW(L"zzhuyin_tsf.dll");
    if (!mod) {
        std::cerr << "LoadLibraryW(zzhuyin_tsf.dll) failed: " << GetLastError() << "\n";
        CoUninitialize();
        return 1;
    }

    using DllGetClassObjectFn = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, void**);
    using DllCanUnloadNowFn   = HRESULT(STDAPICALLTYPE*)();

    auto pGetClassObject = reinterpret_cast<DllGetClassObjectFn>(
        GetProcAddress(mod, "DllGetClassObject"));
    auto pCanUnloadNow = reinterpret_cast<DllCanUnloadNowFn>(
        GetProcAddress(mod, "DllCanUnloadNow"));

    if (!pGetClassObject || !pCanUnloadNow) {
        std::cerr << "Missing COM exports\n";
        FreeLibrary(mod);
        CoUninitialize();
        return 1;
    }

    IClassFactory* factory = nullptr;
    HRESULT hr = pGetClassObject(
        zzhuyin::tsf::kClsidTextService, IID_IClassFactory, reinterpret_cast<void**>(&factory));
    if (FAILED(hr) || !factory) {
        std::cerr << "DllGetClassObject failed: 0x" << std::hex << hr << "\n";
        FreeLibrary(mod);
        CoUninitialize();
        return 1;
    }

    ITfTextInputProcessorEx* tip = nullptr;
    hr = factory->CreateInstance(
        nullptr, IID_ITfTextInputProcessorEx, reinterpret_cast<void**>(&tip));
    if (FAILED(hr) || !tip) {
        std::cerr << "CreateInstance(ITfTextInputProcessorEx) failed\n";
        factory->Release();
        FreeLibrary(mod);
        CoUninitialize();
        return 1;
    }

    // Verify all required TSF interfaces are exposed
    auto check_qi = [&](REFIID iid, const char* name) -> bool {
        IUnknown* unk = nullptr;
        if (FAILED(tip->QueryInterface(iid, reinterpret_cast<void**>(&unk))) || !unk) {
            std::cerr << "QueryInterface failed for " << name << "\n";
            return false;
        }
        unk->Release();
        return true;
    };

    bool ok = true;
    ok &= check_qi(IID_ITfTextInputProcessor, "ITfTextInputProcessor");
    ok &= check_qi(IID_ITfThreadMgrEventSink, "ITfThreadMgrEventSink");
    ok &= check_qi(IID_ITfKeyEventSink, "ITfKeyEventSink");
    ok &= check_qi(IID_ITfCompositionSink, "ITfCompositionSink");
    ok &= check_qi(IID_ITfDisplayAttributeProvider, "ITfDisplayAttributeProvider");
    ok &= check_qi(IID_ITfCompartmentEventSink, "ITfCompartmentEventSink");
    ok &= check_qi(IID_ITfFnConfigure, "ITfFnConfigure");

    // Verify display attribute provider enumeration
    ITfDisplayAttributeProvider* dap = nullptr;
    if (SUCCEEDED(tip->QueryInterface(
            IID_ITfDisplayAttributeProvider, reinterpret_cast<void**>(&dap))) && dap) {
        IEnumTfDisplayAttributeInfo* en = nullptr;
        if (SUCCEEDED(dap->EnumDisplayAttributeInfo(&en)) && en) {
            ITfDisplayAttributeInfo* info = nullptr;
            ULONG fetched = 0;
            if (SUCCEEDED(en->Next(1, &info, &fetched)) && fetched == 1 && info) {
                TF_DISPLAYATTRIBUTE da{};
                info->GetAttributeInfo(&da);
                if (da.lsStyle != TF_LS_DOT) {
                    std::cerr << "Unexpected display attribute line style\n";
                    ok = false;
                }
                info->Release();
            } else {
                std::cerr << "EnumDisplayAttributeInfo::Next failed\n";
                ok = false;
            }
            en->Release();
        } else {
            ok = false;
        }
        dap->Release();
    }

    tip->Release();
    factory->Release();

    // All COM objects released -> DllCanUnloadNow must return S_OK
    if (pCanUnloadNow() != S_OK) {
        std::cerr << "DllCanUnloadNow() != S_OK (refcount leak!)\n";
        ok = false;
    }

    // Verify ConfigStore Registry round-trip on isolated test key
    {
        constexpr wchar_t kTestKey[] = L"Software\\ZZhuyin_UnitTest";
        zzhuyin::tsf::PersistedConfig in_cfg{};
        in_cfg.engine.layout = zzhuyin::KeyboardLayout::Eten;
        in_cfg.engine.prompt_associate_phrase = true;
        in_cfg.engine.charset_filter.include_ext_a = true;
        in_cfg.engine.output_simplified = true;
        in_cfg.engine.orientation = zzhuyin::CandidateOrientation::Horizontal;
        in_cfg.ui.font_size_pt = 16;
        in_cfg.ui.dark_theme = true;

        if (!zzhuyin::tsf::ConfigStore::Save(in_cfg, kTestKey)) {
            std::cerr << "ConfigStore::Save failed\n";
            ok = false;
        } else {
            const auto out_cfg = zzhuyin::tsf::ConfigStore::Load(kTestKey);
            if (out_cfg.engine.layout != zzhuyin::KeyboardLayout::Eten ||
                !out_cfg.engine.prompt_associate_phrase ||
                !out_cfg.engine.charset_filter.include_ext_a ||
                !out_cfg.engine.output_simplified ||
                out_cfg.engine.orientation != zzhuyin::CandidateOrientation::Horizontal ||
                out_cfg.ui.font_size_pt != 16 ||
                !out_cfg.ui.dark_theme) {
                std::cerr << "ConfigStore round-trip mismatch\n";
                ok = false;
            }
        }
        RegDeleteTreeW(HKEY_CURRENT_USER, kTestKey);

        // Verify YAML export/import round-trip
        in_cfg.engine.default_native_mode = false;
        in_cfg.engine.enable_punctuation_input = false;
        in_cfg.engine.enable_warning_beep = false;
        in_cfg.engine.left_shift_toggle_mode = false;
        in_cfg.engine.right_shift_toggle_mode = true;
        in_cfg.engine.charset_filter.output_big5_only = true;
        in_cfg.engine.charset_filter.include_ext_b = true;
        in_cfg.engine.charset_filter.allow_cns = true;
        in_cfg.engine.charset_filter.include_hkscs = true;

        const std::string yaml = zzhuyin::tsf::ConfigStore::ToYaml(in_cfg);
        zzhuyin::tsf::PersistedConfig parsed{};
        if (!zzhuyin::tsf::ConfigStore::FromYaml(yaml, parsed)) {
            std::cerr << "ConfigStore::FromYaml failed\n";
            ok = false;
        } else if (
            parsed.engine.layout != in_cfg.engine.layout ||
            parsed.engine.default_native_mode != in_cfg.engine.default_native_mode ||
            parsed.engine.orientation != in_cfg.engine.orientation ||
            parsed.ui.font_size_pt != in_cfg.ui.font_size_pt ||
            parsed.ui.dark_theme != in_cfg.ui.dark_theme ||
            parsed.engine.prompt_associate_phrase != in_cfg.engine.prompt_associate_phrase ||
            parsed.engine.output_simplified != in_cfg.engine.output_simplified ||
            parsed.engine.enable_punctuation_input != in_cfg.engine.enable_punctuation_input ||
            parsed.engine.enable_warning_beep != in_cfg.engine.enable_warning_beep ||
            parsed.engine.left_shift_toggle_mode != in_cfg.engine.left_shift_toggle_mode ||
            parsed.engine.right_shift_toggle_mode != in_cfg.engine.right_shift_toggle_mode ||
            parsed.engine.charset_filter.output_big5_only != in_cfg.engine.charset_filter.output_big5_only ||
            parsed.engine.charset_filter.include_ext_a != in_cfg.engine.charset_filter.include_ext_a ||
            parsed.engine.charset_filter.include_ext_b != in_cfg.engine.charset_filter.include_ext_b ||
            parsed.engine.charset_filter.allow_cns != in_cfg.engine.charset_filter.allow_cns ||
            parsed.engine.charset_filter.include_hkscs != in_cfg.engine.charset_filter.include_hkscs) {
            std::cerr << "ConfigStore YAML round-trip mismatch\n";
            ok = false;
        }

        zzhuyin::tsf::PersistedConfig bad{};
        if (zzhuyin::tsf::ConfigStore::FromYaml("not_a_valid_config: 123\n", bad)) {
            std::cerr << "ConfigStore::FromYaml should reject unrecognized YAML\n";
            ok = false;
        }
    }

    FreeLibrary(mod);
    CoUninitialize();

    if (ok) {
        std::cout << "[PASS] TSF COM + ConfigStore smoke test\n";
        return 0;
    }
    return 1;
}
