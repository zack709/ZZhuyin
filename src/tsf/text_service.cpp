#include "tsf/text_service.h"

#include "core/imechlex_dict.h"
#include "core/zdict.h"
#include "tsf/display_attr.h"
#include "tsf/edit_session.h"
#include "tsf/guids.h"
#include "tsf/settings_dlg.h"

#include <oleauto.h>
#include <filesystem>
#include <string>
#include <vector>

namespace zzhuyin::tsf {

extern HINSTANCE g_hInst;
extern void DllAddRef();
extern void DllRelease();

namespace {

std::filesystem::path GetModuleDirectory(HINSTANCE hInst) {
    wchar_t buf[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(hInst, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(buf).parent_path();
}

std::filesystem::path GetWindowsDirectoryPath() {
    wchar_t buf[MAX_PATH] = {};
    const UINT len = GetWindowsDirectoryW(buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(buf);
}

std::filesystem::path GetSystemDirectoryPath() {
    wchar_t buf[MAX_PATH] = {};
    const UINT len = GetSystemDirectoryW(buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(buf);
}

std::u16string ConvertToSimplified(const std::u16string& text) {
    if (text.empty()) return text;
    const wchar_t* src = reinterpret_cast<const wchar_t*>(text.data());
    const int src_len = static_cast<int>(text.size());
    const int req = LCMapStringEx(
        L"zh-TW", LCMAP_SIMPLIFIED_CHINESE,
        src, src_len, nullptr, 0, nullptr, nullptr, 0);
    if (req <= 0) return text;

    std::u16string out(static_cast<size_t>(req), u'\0');
    const int written = LCMapStringEx(
        L"zh-TW", LCMAP_SIMPLIFIED_CHINESE,
        src, src_len,
        reinterpret_cast<wchar_t*>(out.data()), req,
        nullptr, nullptr, 0);
    if (written <= 0) return text;
    out.resize(static_cast<size_t>(written));
    return out;
}

zzhuyin::EngineOutput ApplyOutputScript(
    const zzhuyin::EngineOutput& in, bool to_simplified) {
    if (!to_simplified) return in;
    zzhuyin::EngineOutput out = in;
    if (!out.commit_text.empty()) {
        out.commit_text = ConvertToSimplified(out.commit_text);
    }
    if (!out.composition.empty()) {
        out.composition = ConvertToSimplified(out.composition);
    }
    if (out.candidates.active) {
        for (auto& item : out.candidates.page_items) {
            item = ConvertToSimplified(item);
        }
    }
    return out;
}

} // namespace

TextService::TextService() {
    DllAddRef();
}

TextService::~TextService() {
    if (composition_) {
        composition_->Release();
        composition_ = nullptr;
    }
    if (active_context_) {
        active_context_->Release();
        active_context_ = nullptr;
    }
    DllRelease();
}

STDMETHODIMP TextService::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfTextInputProcessor) ||
        IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppvObj = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
        *ppvObj = static_cast<ITfDisplayAttributeProvider*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompartmentEventSink)) {
        *ppvObj = static_cast<ITfCompartmentEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfFunction) || IsEqualIID(riid, IID_ITfFnConfigure)) {
        *ppvObj = static_cast<ITfFnConfigure*>(this);
    } else {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) TextService::AddRef() {
    return static_cast<ULONG>(++ref_count_);
}

STDMETHODIMP_(ULONG) TextService::Release() {
    const long count = --ref_count_;
    if (count == 0) {
        delete this;
    }
    return static_cast<ULONG>(count);
}

STDMETHODIMP TextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
    return ActivateEx(ptim, tid, 0);
}

STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD /*dwFlags*/) {
    if (!ptim) return E_INVALIDARG;

    thread_mgr_ = ptim;
    thread_mgr_->AddRef();
    client_id_ = tid;

    InitDictionary();
    config_ = ConfigStore::Load();
    engine_ = std::make_unique<zzhuyin::ZhuyinEngine>(*dict_, config_.engine);

    InitDisplayAttributeAtom();

    if (!InitThreadMgrEventSink()) {
        Deactivate();
        return E_FAIL;
    }
    if (!InitKeyEventSink()) {
        Deactivate();
        return E_FAIL;
    }

    candidate_wnd_.Create(g_hInst);
    candidate_wnd_.SetOrientation(config_.engine.orientation);
    candidate_wnd_.SetFontSizePt(config_.ui.font_size_pt);
    candidate_wnd_.SetDarkTheme(config_.ui.dark_theme);
    candidate_wnd_.SetSelectCallback([this](size_t page_offset) {
        OnCandidateMouseSelect(page_offset);
    });

    symbol_wnd_.Create(g_hInst);
    symbol_wnd_.SetDarkTheme(config_.ui.dark_theme);
    symbol_wnd_.SetSymbolCallback([this](char16_t sym) {
        OnSymbolMouseSelect(sym);
    });
    symbol_wnd_.SetCloseCallback([this]() {
        OnSymbolKeyboardClose();
    });

    InitOpenCloseCompartment();
    InitLangBar();

    return S_OK;
}

STDMETHODIMP TextService::Deactivate() {
    if (active_context_) {
        TerminateComposition(active_context_);
        active_context_->Release();
        active_context_ = nullptr;
    }

    UninitLangBar();
    UninitOpenCloseCompartment();
    symbol_wnd_.Destroy();
    candidate_wnd_.Destroy();
    UninitKeyEventSink();
    UninitThreadMgrEventSink();

    engine_.reset();

    if (composition_) {
        composition_->Release();
        composition_ = nullptr;
    }
    if (thread_mgr_) {
        thread_mgr_->Release();
        thread_mgr_ = nullptr;
    }
    client_id_ = TF_CLIENTID_NULL;
    return S_OK;
}

bool TextService::InitDictionary() {
    const auto dll_dir = GetModuleDirectory(g_hInst);
    const auto win_dir = GetWindowsDirectoryPath();
    const auto sys_dir = GetSystemDirectoryPath();

    auto try_load_imd = [&](const std::filesystem::path& imd_path) -> bool {
        if (imd_path.empty() || !std::filesystem::exists(imd_path)) return false;
        auto imd = std::make_unique<zzhuyin::ImechlexDictionary>();
        if (!imd->LoadImechlex(imd_path)) return false;

        const std::vector<std::pair<std::filesystem::path, std::filesystem::path>> assoc_candidates = {
            {dll_dir / L"lcptr.tbl", dll_dir / L"lcphrase.tbl"},
            {dll_dir / L".." / L"lcptr.tbl", dll_dir / L".." / L"lcphrase.tbl"},
            {dll_dir / L".." / L"targets" / L"lcptr.tbl", dll_dir / L".." / L"targets" / L"lcphrase.tbl"},
            {sys_dir / L"lcptr.tbl", sys_dir / L"lcphrase.tbl"},
        };
        for (const auto& [ptr_p, phr_p] : assoc_candidates) {
            if (!ptr_p.empty() && std::filesystem::exists(ptr_p) && std::filesystem::exists(phr_p)) {
                if (imd->LoadAssociateTables(ptr_p, phr_p)) break;
            }
        }
        dict_ = std::move(imd);
        return true;
    };

    auto try_load_zdict = [&](const std::filesystem::path& zdict_path) -> bool {
        if (zdict_path.empty() || !std::filesystem::exists(zdict_path)) return false;
        auto zd = std::make_unique<zzhuyin::ZDictDictionary>();
        if (!zd->Load(zdict_path)) return false;
        dict_ = std::move(zd);
        return true;
    };

    // 1. Local IMTCPH.IMD (100% Win7 compatible mode)
    for (const auto& path : {
            dll_dir / L"DICTS" / L"IMTCPH.IMD",
            dll_dir / L"IMTCPH.IMD",
            dll_dir / L".." / L"DICTS" / L"IMTCPH.IMD",
            dll_dir / L".." / L"targets" / L"DICTS" / L"IMTCPH.IMD"}) {
        if (try_load_imd(path)) return true;
    }

    // 2. Local standalone zzhuyin.zdict (open-source / bundled mode)
    for (const auto& path : {
            dll_dir / L"zzhuyin.zdict",
            dll_dir / L"DICTS" / L"zzhuyin.zdict",
            dll_dir / L".." / L"zzhuyin.zdict"}) {
        if (try_load_zdict(path)) return true;
    }

    // 3. System Windows IME IMTCPH.IMD fallback
    if (try_load_imd(win_dir / L"IME" / L"IMETC10" / L"DICTS" / L"IMTCPH.IMD")) {
        return true;
    }

    // Fallback to empty dictionary so engine_ always has a valid reference
    dict_ = std::make_unique<zzhuyin::ZDictDictionary>();
    return false;
}

bool TextService::InitDisplayAttributeAtom() {
    ITfCategoryMgr* cat_mgr = nullptr;
    if (FAILED(CoCreateInstance(
            CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
            IID_ITfCategoryMgr, reinterpret_cast<void**>(&cat_mgr)))) {
        return false;
    }
    const HRESULT hr = cat_mgr->RegisterGUID(kGuidDisplayAttributeInput, &display_attr_atom_);
    cat_mgr->Release();
    return SUCCEEDED(hr);
}

bool TextService::InitThreadMgrEventSink() {
    ITfSource* source = nullptr;
    if (FAILED(thread_mgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
        return false;
    }
    const HRESULT hr = source->AdviseSink(
        IID_ITfThreadMgrEventSink,
        static_cast<ITfThreadMgrEventSink*>(this),
        &thread_mgr_sink_cookie_);
    source->Release();
    return SUCCEEDED(hr);
}

void TextService::UninitThreadMgrEventSink() {
    if (thread_mgr_ && thread_mgr_sink_cookie_ != TF_INVALID_COOKIE) {
        ITfSource* source = nullptr;
        if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
            source->UnadviseSink(thread_mgr_sink_cookie_);
            source->Release();
        }
        thread_mgr_sink_cookie_ = TF_INVALID_COOKIE;
    }
}

bool TextService::InitKeyEventSink() {
    ITfKeystrokeMgr* ks_mgr = nullptr;
    if (FAILED(thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&ks_mgr)))) {
        return false;
    }
    const HRESULT hr = ks_mgr->AdviseKeyEventSink(
        client_id_, static_cast<ITfKeyEventSink*>(this), TRUE);
    if (SUCCEEDED(hr)) {
        TF_PRESERVEDKEY pk{};
        pk.uVKey = VK_SPACE;
        pk.uModifiers = TF_MOD_CONTROL;
        constexpr wchar_t kDesc[] = L"Toggle Chinese/English";
        ks_mgr->PreserveKey(
            client_id_, kGuidPreservedKeyToggleNative, &pk,
            kDesc, static_cast<ULONG>(wcslen(kDesc)));

        TF_PRESERVEDKEY pk_sym{};
        pk_sym.uVKey = VK_OEM_COMMA;
        pk_sym.uModifiers = TF_MOD_CONTROL | TF_MOD_ALT;
        constexpr wchar_t kSymDesc[] = L"Toggle Symbol Keyboard";
        ks_mgr->PreserveKey(
            client_id_, kGuidPreservedKeySymbolKeyboard, &pk_sym,
            kSymDesc, static_cast<ULONG>(wcslen(kSymDesc)));
    }
    ks_mgr->Release();
    return SUCCEEDED(hr);
}

void TextService::UninitKeyEventSink() {
    if (thread_mgr_ && client_id_ != TF_CLIENTID_NULL) {
        ITfKeystrokeMgr* ks_mgr = nullptr;
        if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&ks_mgr)))) {
            TF_PRESERVEDKEY pk{};
            pk.uVKey = VK_SPACE;
            pk.uModifiers = TF_MOD_CONTROL;
            ks_mgr->UnpreserveKey(kGuidPreservedKeyToggleNative, &pk);

            TF_PRESERVEDKEY pk_sym{};
            pk_sym.uVKey = VK_OEM_COMMA;
            pk_sym.uModifiers = TF_MOD_CONTROL | TF_MOD_ALT;
            ks_mgr->UnpreserveKey(kGuidPreservedKeySymbolKeyboard, &pk_sym);

            ks_mgr->UnadviseKeyEventSink(client_id_);
            ks_mgr->Release();
        }
    }
}

void TextService::InitOpenCloseCompartment() {
    if (!thread_mgr_ || open_close_comp_) return;

    ITfCompartmentMgr* comp_mgr = nullptr;
    if (FAILED(thread_mgr_->QueryInterface(
            IID_ITfCompartmentMgr, reinterpret_cast<void**>(&comp_mgr))) || !comp_mgr) {
        return;
    }

    if (SUCCEEDED(comp_mgr->GetCompartment(
            GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &open_close_comp_)) && open_close_comp_) {
        SyncOpenCloseCompartment();

        ITfSource* source = nullptr;
        if (SUCCEEDED(open_close_comp_->QueryInterface(
                IID_ITfSource, reinterpret_cast<void**>(&source))) && source) {
            source->AdviseSink(
                IID_ITfCompartmentEventSink,
                static_cast<ITfCompartmentEventSink*>(this),
                &open_close_sink_cookie_);
            source->Release();
        }
    }
    comp_mgr->Release();
}

void TextService::UninitOpenCloseCompartment() {
    if (!open_close_comp_) return;
    if (open_close_sink_cookie_ != TF_INVALID_COOKIE) {
        ITfSource* source = nullptr;
        if (SUCCEEDED(open_close_comp_->QueryInterface(
                IID_ITfSource, reinterpret_cast<void**>(&source))) && source) {
            source->UnadviseSink(open_close_sink_cookie_);
            source->Release();
        }
        open_close_sink_cookie_ = TF_INVALID_COOKIE;
    }
    open_close_comp_->Release();
    open_close_comp_ = nullptr;
}

void TextService::SyncOpenCloseCompartment() {
    if (!open_close_comp_ || !engine_ || updating_compartment_) return;
    updating_compartment_ = true;
    VARIANT var{};
    var.vt = VT_I4;
    var.lVal = engine_->IsNativeMode() ? 1 : 0;
    open_close_comp_->SetValue(client_id_, &var);
    updating_compartment_ = false;
}

STDMETHODIMP TextService::OnChange(REFGUID rguid) {
    if (updating_compartment_ || !open_close_comp_ || !engine_) {
        return S_OK;
    }
    if (IsEqualGUID(rguid, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)) {
        VARIANT var{};
        if (SUCCEEDED(open_close_comp_->GetValue(&var)) && var.vt == VT_I4) {
            const bool is_native = (var.lVal != 0);
            if (engine_->IsNativeMode() != is_native) {
                engine_->SetNativeMode(is_native);
                candidate_wnd_.Hide();
                if (active_context_) {
                    TerminateComposition(active_context_);
                }
                if (lang_bar_btn_) {
                    lang_bar_btn_->NotifyUpdate();
                }
            }
        }
    }
    return S_OK;
}

void TextService::InitLangBar() {
    if (!thread_mgr_ || lang_bar_btn_) return;

    ITfLangBarItemMgr* mgr = nullptr;
    if (FAILED(thread_mgr_->QueryInterface(
            IID_ITfLangBarItemMgr, reinterpret_cast<void**>(&mgr))) || !mgr) {
        return;
    }

    LangBarCallbacks cbs;
    cbs.is_native = [this]() { return engine_ ? engine_->IsNativeMode() : true; };
    cbs.is_full_shape = [this]() { return engine_ ? engine_->IsFullShapeMode() : false; };
    cbs.get_config = [this]() { return config_; };
    cbs.toggle_native = [this]() {
        if (engine_) {
            engine_->SetNativeMode(!engine_->IsNativeMode());
            candidate_wnd_.Hide();
            SyncOpenCloseCompartment();
        }
    };
    cbs.toggle_full_shape = [this]() {
        if (engine_) {
            engine_->SetFullShapeMode(!engine_->IsFullShapeMode());
        }
    };
    cbs.update_config = [this](const PersistedConfig& cfg) {
        ApplyPersistedConfig(cfg);
        ConfigStore::Save(cfg);
    };
    cbs.open_settings = [this]() {
        OpenSettingsDialog();
    };

    lang_bar_btn_ = new (std::nothrow) LangBarButton(std::move(cbs));
    if (lang_bar_btn_) {
        if (FAILED(mgr->AddItem(lang_bar_btn_))) {
            lang_bar_btn_->Release();
            lang_bar_btn_ = nullptr;
        }
    }
    mgr->Release();
}

void TextService::UninitLangBar() {
    if (!lang_bar_btn_) return;
    if (thread_mgr_) {
        ITfLangBarItemMgr* mgr = nullptr;
        if (SUCCEEDED(thread_mgr_->QueryInterface(
                IID_ITfLangBarItemMgr, reinterpret_cast<void**>(&mgr))) && mgr) {
            mgr->RemoveItem(lang_bar_btn_);
            mgr->Release();
        }
    }
    lang_bar_btn_->Release();
    lang_bar_btn_ = nullptr;
}

void TextService::ApplyPersistedConfig(const PersistedConfig& cfg) {
    config_ = cfg;
    if (engine_) {
        engine_->SetConfig(cfg.engine);
    }
    candidate_wnd_.SetOrientation(cfg.engine.orientation);
    candidate_wnd_.SetFontSizePt(cfg.ui.font_size_pt);
    candidate_wnd_.SetDarkTheme(cfg.ui.dark_theme);
    symbol_wnd_.SetDarkTheme(cfg.ui.dark_theme);
}

// ITfThreadMgrEventSink
STDMETHODIMP TextService::OnInitDocumentMgr(ITfDocumentMgr* /*pdim*/) { return S_OK; }
STDMETHODIMP TextService::OnUninitDocumentMgr(ITfDocumentMgr* /*pdim*/) { return S_OK; }
STDMETHODIMP TextService::OnPushContext(ITfContext* /*pic*/) { return S_OK; }
STDMETHODIMP TextService::OnPopContext(ITfContext* /*pic*/) { return S_OK; }

STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr* /*pdimPrevFocus*/) {
    if (engine_) {
        engine_->Reset();
        engine_->SetSymbolKeyboardActive(false);
        ApplyPersistedConfig(ConfigStore::Load());
    }
    candidate_wnd_.Hide();
    symbol_wnd_.Hide();

    if (active_context_) {
        active_context_->Release();
        active_context_ = nullptr;
    }
    if (pdimFocus) {
        pdimFocus->GetTop(&active_context_);
    }
    return S_OK;
}

// ITfKeyEventSink
STDMETHODIMP TextService::OnSetFocus(BOOL fForeground) {
    if (!fForeground) {
        if (engine_) {
            engine_->Reset();
            engine_->SetSymbolKeyboardActive(false);
        }
        candidate_wnd_.Hide();
        symbol_wnd_.Hide();
    }
    return S_OK;
}

zzhuyin::KeyEvent TextService::TranslateKeyEvent(
    WPARAM wParam, LPARAM lParam, bool is_key_up) const {
    zzhuyin::KeyEvent ev{};
    ev.is_key_up = is_key_up;
    ev.timestamp_ms = GetTickCount64();

    UINT vk = static_cast<UINT>(wParam & 0xFFFF);
    const UINT scan_code = static_cast<UINT>((lParam >> 16) & 0xFF);
    const bool is_extended = ((lParam & (1LL << 24)) != 0);

    // TSF sends generic VK_SHIFT / VK_CONTROL; resolve Left/Right via scan code
    if (vk == VK_SHIFT) {
        const UINT mapped = MapVirtualKeyW(scan_code, MAPVK_VSC_TO_VK_EX);
        if (mapped == VK_LSHIFT || mapped == VK_RSHIFT) {
            vk = mapped;
        }
    } else if (vk == VK_CONTROL) {
        vk = is_extended ? VK_RCONTROL : VK_LCONTROL;
    }

    ev.vk_code = static_cast<uint16_t>(vk);

    BYTE key_state[256] = {};
    if (GetKeyboardState(key_state)) {
        if (key_state[VK_SHIFT]   & 0x80) ev.modifiers |= zzhuyin::mod::kShift;
        if (key_state[VK_CONTROL] & 0x80) ev.modifiers |= zzhuyin::mod::kControl;
        if (key_state[VK_MENU]    & 0x80) ev.modifiers |= zzhuyin::mod::kAlt;
        if (key_state[VK_CAPITAL] & 0x01) ev.modifiers |= zzhuyin::mod::kCapsLock;
        if (key_state[VK_KANA]    & 0x01) ev.modifiers |= zzhuyin::mod::kKana;

        if (!is_key_up && vk != VK_MENU) {
            wchar_t ch_buf[4] = {};
            const int res = ToUnicode(
                static_cast<UINT>(wParam & 0xFFFF), scan_code, key_state, ch_buf, 4, 0);
            if (res == 1) {
                ev.unicode_char = static_cast<char16_t>(ch_buf[0]);
            }
        }
    }

    return ev;
}

STDMETHODIMP TextService::OnTestKeyDown(
    ITfContext* /*pic*/, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    if (!engine_) {
        *pfEaten = FALSE;
        return S_OK;
    }
    const auto ev = TranslateKeyEvent(wParam, lParam, /*is_key_up=*/false);
    *pfEaten = engine_->WillHandleKey(ev) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(
    ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    if (!engine_) {
        *pfEaten = FALSE;
        return S_OK;
    }
    const auto ev = TranslateKeyEvent(wParam, lParam, /*is_key_up=*/false);
    const auto out = engine_->ProcessKey(ev);
    *pfEaten = out.handled ? TRUE : FALSE;
    ApplyOutput(pic, out);
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(
    ITfContext* /*pic*/, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    if (!engine_) {
        *pfEaten = FALSE;
        return S_OK;
    }
    const auto ev = TranslateKeyEvent(wParam, lParam, /*is_key_up=*/true);
    *pfEaten = engine_->WillHandleKey(ev) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(
    ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    if (!engine_) {
        *pfEaten = FALSE;
        return S_OK;
    }
    const auto ev = TranslateKeyEvent(wParam, lParam, /*is_key_up=*/true);
    const auto out = engine_->ProcessKey(ev);
    *pfEaten = out.handled ? TRUE : FALSE;
    ApplyOutput(pic, out);
    return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(
    ITfContext* pic, REFGUID rguid, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    if (IsEqualGUID(rguid, kGuidPreservedKeyToggleNative) && engine_) {
        engine_->SetNativeMode(!engine_->IsNativeMode());
        candidate_wnd_.Hide();
        symbol_wnd_.Hide();
        if (pic) {
            TerminateComposition(pic);
        }
        SyncOpenCloseCompartment();
        if (lang_bar_btn_) {
            lang_bar_btn_->NotifyUpdate();
        }
        *pfEaten = TRUE;
    } else if (IsEqualGUID(rguid, kGuidPreservedKeySymbolKeyboard) && engine_) {
        if (!engine_->IsNativeMode()) {
            engine_->SetNativeMode(true);
            SyncOpenCloseCompartment();
            if (lang_bar_btn_) lang_bar_btn_->NotifyUpdate();
        }
        zzhuyin::KeyEvent ev{};
        ev.vk_code = zzhuyin::vk::kOemComma;
        ev.modifiers = static_cast<uint16_t>(zzhuyin::mod::kControl | zzhuyin::mod::kAlt);
        const auto out = engine_->ProcessKey(ev);
        ApplyOutput(pic ? pic : active_context_, out);
        *pfEaten = TRUE;
    }
    return S_OK;
}

void TextService::ApplyOutput(ITfContext* context, const zzhuyin::EngineOutput& out) {
    if (out.beep) {
        MessageBeep(MB_OK);
    }
    if (out.mode_changed) {
        SyncOpenCloseCompartment();
        if (lang_bar_btn_) {
            lang_bar_btn_->NotifyUpdate();
        }
    }
    if (out.symbol_keyboard_active) {
        if (!symbol_wnd_.IsVisible()) {
            symbol_wnd_.Show();
        }
    } else {
        if (symbol_wnd_.IsVisible()) {
            symbol_wnd_.Hide();
        }
    }

    if (!context) {
        if (!out.candidates.active) {
            candidate_wnd_.Hide();
        }
        return;
    }

    if (active_context_ != context) {
        if (active_context_) active_context_->Release();
        active_context_ = context;
        active_context_->AddRef();
    }

    const zzhuyin::EngineOutput final_out =
        ApplyOutputScript(out, config_.engine.output_simplified);

    const bool need_edit_session =
        !final_out.commit_text.empty() ||
        !final_out.composition.empty() ||
        (composition_ != nullptr) ||
        final_out.candidates.active;

    if (need_edit_session) {
        auto* session = new (std::nothrow) CallbackEditSession(
            [this, context, final_out](TfEditCookie ec) -> HRESULT {
                return ApplyOutputInEditSession(ec, context, final_out);
            });
        if (session) {
            HRESULT hr_session = S_OK;
            context->RequestEditSession(
                client_id_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr_session);
            session->Release();
        }
    } else {
        candidate_wnd_.Hide();
    }
}

void TextService::SetCompositionDisplayAttribute(
    TfEditCookie ec, ITfContext* context, ITfRange* range) {
    if (display_attr_atom_ == TF_INVALID_GUIDATOM || !context || !range) {
        return;
    }
    ITfProperty* prop = nullptr;
    if (SUCCEEDED(context->GetProperty(GUID_PROP_ATTRIBUTE, &prop))) {
        VARIANT var{};
        var.vt = VT_I4;
        var.lVal = static_cast<LONG>(display_attr_atom_);
        prop->SetValue(ec, range, &var);
        prop->Release();
    }
}

void TextService::ClearCompositionDisplayAttribute(
    TfEditCookie ec, ITfContext* context, ITfRange* range) {
    if (!context || !range) {
        return;
    }
    ITfProperty* prop = nullptr;
    if (SUCCEEDED(context->GetProperty(GUID_PROP_ATTRIBUTE, &prop))) {
        prop->Clear(ec, range);
        prop->Release();
    }
}

HRESULT TextService::ApplyOutputInEditSession(
    TfEditCookie ec, ITfContext* context, const zzhuyin::EngineOutput& out) {
    // 1. Commit finalized text if present
    if (!out.commit_text.empty()) {
        const wchar_t* commit_ptr = reinterpret_cast<const wchar_t*>(out.commit_text.data());
        const LONG commit_len = static_cast<LONG>(out.commit_text.size());

        if (composition_) {
            ITfRange* range = nullptr;
            if (SUCCEEDED(composition_->GetRange(&range))) {
                ClearCompositionDisplayAttribute(ec, context, range);
                range->SetText(ec, 0, commit_ptr, commit_len);
                range->Collapse(ec, TF_ANCHOR_END);
                TF_SELECTION sel{};
                sel.range = range;
                sel.style.ase = TF_AE_NONE;
                sel.style.fInterimChar = FALSE;
                context->SetSelection(ec, 1, &sel);
                range->Release();
            }
            composition_->EndComposition(ec);
            composition_->Release();
            composition_ = nullptr;
        } else {
            ITfInsertAtSelection* insert_sel = nullptr;
            if (SUCCEEDED(context->QueryInterface(
                    IID_ITfInsertAtSelection, reinterpret_cast<void**>(&insert_sel)))) {
                ITfRange* range = nullptr;
                if (SUCCEEDED(insert_sel->InsertTextAtSelection(
                        ec, 0, commit_ptr, commit_len, &range)) && range) {
                    range->Collapse(ec, TF_ANCHOR_END);
                    TF_SELECTION sel{};
                    sel.range = range;
                    sel.style.ase = TF_AE_NONE;
                    sel.style.fInterimChar = FALSE;
                    context->SetSelection(ec, 1, &sel);
                    range->Release();
                }
                insert_sel->Release();
            }
        }
    }

    // 2. Update or end active composition string
    if (!out.composition.empty()) {
        const wchar_t* comp_ptr = reinterpret_cast<const wchar_t*>(out.composition.data());
        const LONG comp_len = static_cast<LONG>(out.composition.size());

        if (!composition_) {
            ITfInsertAtSelection* insert_sel = nullptr;
            ITfContextComposition* ctx_comp = nullptr;
            if (SUCCEEDED(context->QueryInterface(
                    IID_ITfInsertAtSelection, reinterpret_cast<void**>(&insert_sel))) &&
                SUCCEEDED(context->QueryInterface(
                    IID_ITfContextComposition, reinterpret_cast<void**>(&ctx_comp)))) {
                ITfRange* range = nullptr;
                if (SUCCEEDED(insert_sel->InsertTextAtSelection(
                        ec, TF_IAS_QUERYONLY, nullptr, 0, &range)) && range) {
                    ctx_comp->StartComposition(
                        ec, range, static_cast<ITfCompositionSink*>(this), &composition_);
                    range->Release();
                }
            }
            if (ctx_comp) ctx_comp->Release();
            if (insert_sel) insert_sel->Release();
        }

        if (composition_) {
            ITfRange* range = nullptr;
            if (SUCCEEDED(composition_->GetRange(&range))) {
                range->SetText(ec, 0, comp_ptr, comp_len);
                SetCompositionDisplayAttribute(ec, context, range);

                ITfRange* caret_range = nullptr;
                if (SUCCEEDED(range->Clone(&caret_range))) {
                    caret_range->Collapse(ec, TF_ANCHOR_END);
                    TF_SELECTION sel{};
                    sel.range = caret_range;
                    sel.style.ase = TF_AE_NONE;
                    sel.style.fInterimChar = FALSE;
                    context->SetSelection(ec, 1, &sel);
                    caret_range->Release();
                }
                range->Release();
            }
        }
    } else if (composition_) {
        ITfRange* range = nullptr;
        if (SUCCEEDED(composition_->GetRange(&range))) {
            ClearCompositionDisplayAttribute(ec, context, range);
            range->SetText(ec, 0, L"", 0);
            range->Release();
        }
        composition_->EndComposition(ec);
        composition_->Release();
        composition_ = nullptr;
    }

    // 3. Update or hide floating candidate window
    if (out.candidates.active) {
        RECT caret_rc{100, 100, 100, 124};
        ITfContextView* view = nullptr;
        if (SUCCEEDED(context->GetActiveView(&view)) && view) {
            TF_SELECTION sel{};
            ULONG fetched = 0;
            if (SUCCEEDED(context->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &fetched)) &&
                fetched > 0 && sel.range) {
                BOOL clipped = FALSE;
                view->GetTextExt(ec, sel.range, &caret_rc, &clipped);
                sel.range->Release();
            }
            view->Release();
        }
        candidate_wnd_.Update(out.candidates, caret_rc);
    } else {
        candidate_wnd_.Hide();
    }

    return S_OK;
}

void TextService::TerminateComposition(ITfContext* context) {
    if (!composition_ || !context) return;
    auto* session = new (std::nothrow) CallbackEditSession(
        [this, context](TfEditCookie ec) -> HRESULT {
            if (composition_) {
                ITfRange* range = nullptr;
                if (SUCCEEDED(composition_->GetRange(&range))) {
                    ClearCompositionDisplayAttribute(ec, context, range);
                    range->SetText(ec, 0, L"", 0);
                    range->Release();
                }
                composition_->EndComposition(ec);
                composition_->Release();
                composition_ = nullptr;
            }
            return S_OK;
        });
    if (session) {
        HRESULT hr_session = S_OK;
        context->RequestEditSession(
            client_id_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr_session);
        session->Release();
    }
}

void TextService::OnCandidateMouseSelect(size_t page_offset) {
    if (!engine_ || page_offset >= 9) return;

    zzhuyin::KeyEvent ev{};
    ev.vk_code = static_cast<uint16_t>('1' + page_offset);
    ev.unicode_char = static_cast<char16_t>('1' + page_offset);
    if (engine_->State() == zzhuyin::EngineState::CandidateAssociate) {
        ev.modifiers = zzhuyin::mod::kShift;
    }

    const auto out = engine_->ProcessKey(ev);
    ApplyOutput(active_context_, out);
}

void TextService::OnSymbolMouseSelect(char16_t sym) {
    if (!engine_ || sym == 0) return;

    zzhuyin::EngineOutput out{};
    out.handled = true;
    out.commit_text.assign(1, sym);
    out.symbol_keyboard_active = engine_->IsSymbolKeyboardActive();
    ApplyOutput(active_context_, out);
}

void TextService::OnSymbolKeyboardClose() {
    if (engine_) {
        engine_->SetSymbolKeyboardActive(false);
    }
    symbol_wnd_.Hide();
}

// ITfCompositionSink
STDMETHODIMP TextService::OnCompositionTerminated(
    TfEditCookie /*ecWrite*/, ITfComposition* pComposition) {
    if (composition_ && composition_ == pComposition) {
        composition_->Release();
        composition_ = nullptr;
    }
    if (engine_) {
        engine_->Reset();
    }
    candidate_wnd_.Hide();
    return S_OK;
}

// ITfDisplayAttributeProvider
STDMETHODIMP TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) {
    if (!ppEnum) return E_INVALIDARG;
    auto* enumerator = new (std::nothrow) zzhuyin::tsf::EnumDisplayAttributeInfo();
    if (!enumerator) return E_OUTOFMEMORY;
    *ppEnum = enumerator;
    return S_OK;
}

STDMETHODIMP TextService::GetDisplayAttributeInfo(
    REFGUID guid, ITfDisplayAttributeInfo** ppInfo) {
    if (!ppInfo) return E_INVALIDARG;
    *ppInfo = nullptr;
    if (IsEqualGUID(guid, kGuidDisplayAttributeInput)) {
        auto* info = new (std::nothrow) DisplayAttributeInfo();
        if (!info) return E_OUTOFMEMORY;
        *ppInfo = info;
        return S_OK;
    }
    return E_INVALIDARG;
}

void TextService::OpenSettingsDialog() {
    this->AddRef();
    std::shared_ptr<TextService> self(this, [](TextService* p) { p->Release(); });
    ShowSettingsDialog(g_hInst, [self](const PersistedConfig& cfg) {
        self->ApplyPersistedConfig(cfg);
    });
}

// ITfFunction / ITfFnConfigure
STDMETHODIMP TextService::GetDisplayName(BSTR* pbstrName) {
    if (!pbstrName) return E_INVALIDARG;
    *pbstrName = SysAllocString(L"Z注音 設定");
    return *pbstrName ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP TextService::Show(HWND /*hwndParent*/, LANGID /*langid*/, REFGUID /*rguidProfile*/) {
    OpenSettingsDialog();
    return S_OK;
}

} // namespace zzhuyin::tsf
