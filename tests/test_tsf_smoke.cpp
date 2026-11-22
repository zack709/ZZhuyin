#include "tsf/config_reg.h"
#include "tsf/guids.h"

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
        in_cfg.engine.orientation = zzhuyin::CandidateOrientation::Horizontal;
        in_cfg.ui.font_size_pt = 16;

        if (!zzhuyin::tsf::ConfigStore::Save(in_cfg, kTestKey)) {
            std::cerr << "ConfigStore::Save failed\n";
            ok = false;
        } else {
            const auto out_cfg = zzhuyin::tsf::ConfigStore::Load(kTestKey);
            if (out_cfg.engine.layout != zzhuyin::KeyboardLayout::Eten ||
                !out_cfg.engine.prompt_associate_phrase ||
                !out_cfg.engine.charset_filter.include_ext_a ||
                out_cfg.engine.orientation != zzhuyin::CandidateOrientation::Horizontal ||
                out_cfg.ui.font_size_pt != 16) {
                std::cerr << "ConfigStore round-trip mismatch\n";
                ok = false;
            }
        }
        RegDeleteTreeW(HKEY_CURRENT_USER, kTestKey);
    }

    FreeLibrary(mod);
    CoUninitialize();

    if (ok) {
        std::cout << "[PASS] TSF COM + ConfigStore smoke test\n";
        return 0;
    }
    return 1;
}
