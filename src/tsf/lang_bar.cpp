#include "tsf/lang_bar.h"

#include "tsf/guids.h"

#include <oleauto.h>
#include <olectl.h>
#include <cstring>

namespace zzhuyin::tsf {

extern void DllAddRef();
extern void DllRelease();

namespace {

constexpr DWORD kLangBarSinkCookie = 0x5A5A0001;

enum MenuId : UINT {
    kMenuToggleNative    = 101,
    kMenuToggleFullShape = 102,
    kMenuLayoutStandard  = 201,
    kMenuLayoutEten      = 202,
    kMenuLayoutIbm       = 203,
    kMenuLayoutGinYieh   = 204,
    kMenuOrientVertical  = 301,
    kMenuOrientHoriz     = 302,
    kMenuToggleAssociate = 401,
    kMenuToggleBig5Only  = 402,
    kMenuToggleExtA      = 403,
    kMenuToggleCns       = 404,
    kMenuToggleBeep      = 405,
};

void AddMenuCheckItem(ITfMenu* menu, UINT id, const wchar_t* text, bool checked) {
    const DWORD flags = checked ? TF_LBMENUF_CHECKED : 0;
    menu->AddMenuItem(
        id, flags, nullptr, nullptr, text, static_cast<ULONG>(wcslen(text)), nullptr);
}

void AddMenuSeparator(ITfMenu* menu) {
    menu->AddMenuItem(0, TF_LBMENUF_SEPARATOR, nullptr, nullptr, nullptr, 0, nullptr);
}

} // namespace

LangBarButton::LangBarButton(LangBarCallbacks callbacks)
    : cb_(std::move(callbacks)) {
    DllAddRef();
}

LangBarButton::~LangBarButton() {
    if (sink_) {
        sink_->Release();
        sink_ = nullptr;
    }
    DllRelease();
}

STDMETHODIMP LangBarButton::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfLangBarItem) ||
        IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppvObj = static_cast<ITfLangBarItemButton*>(this);
    } else if (IsEqualIID(riid, IID_ITfSource)) {
        *ppvObj = static_cast<ITfSource*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) LangBarButton::AddRef() {
    return static_cast<ULONG>(++ref_count_);
}

STDMETHODIMP_(ULONG) LangBarButton::Release() {
    const long count = --ref_count_;
    if (count == 0) {
        delete this;
    }
    return static_cast<ULONG>(count);
}

STDMETHODIMP LangBarButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
    if (!pInfo) return E_INVALIDARG;
    pInfo->clsidService = kClsidTextService;
    pInfo->guidItem     = kGuidLbiInputMode;
    pInfo->dwStyle      = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_SHOWNINTRAY;
    pInfo->ulSort       = 0;
    wcsncpy_s(pInfo->szDescription, L"Z注音 中英切換", _TRUNCATE);
    return S_OK;
}

STDMETHODIMP LangBarButton::GetStatus(DWORD* pdwStatus) {
    if (!pdwStatus) return E_INVALIDARG;
    *pdwStatus = 0;
    return S_OK;
}

STDMETHODIMP LangBarButton::Show(BOOL /*fShow*/) {
    return E_NOTIMPL;
}

STDMETHODIMP LangBarButton::GetTooltipString(BSTR* pbstrToolTip) {
    if (!pbstrToolTip) return E_INVALIDARG;
    const bool native = cb_.is_native ? cb_.is_native() : true;
    const bool full   = cb_.is_full_shape ? cb_.is_full_shape() : false;
    const wchar_t* tip = native
        ? (full ? L"Z注音（中文 / 全形）" : L"Z注音（中文 / 半形）")
        : (full ? L"Z注音（英文 / 全形）" : L"Z注音（英文 / 半形）");
    *pbstrToolTip = SysAllocString(tip);
    return *pbstrToolTip ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LangBarButton::OnClick(TfLBIClick click, POINT /*pt*/, const RECT* /*prcArea*/) {
    if (click == TF_LBI_CLK_LEFT && cb_.toggle_native) {
        cb_.toggle_native();
        NotifyUpdate();
    }
    return S_OK;
}

STDMETHODIMP LangBarButton::InitMenu(ITfMenu* pMenu) {
    if (!pMenu) return E_INVALIDARG;

    const bool native = cb_.is_native ? cb_.is_native() : true;
    const bool full   = cb_.is_full_shape ? cb_.is_full_shape() : false;
    const PersistedConfig cfg = cb_.get_config ? cb_.get_config() : PersistedConfig{};

    AddMenuCheckItem(pMenu, kMenuToggleNative, native ? L"中文模式（點擊切換英文）" : L"英文模式（點擊切換中文）", native);
    AddMenuCheckItem(pMenu, kMenuToggleFullShape, L"全形模式", full);
    AddMenuSeparator(pMenu);

    AddMenuCheckItem(pMenu, kMenuLayoutStandard, L"鍵盤：標準大千", cfg.engine.layout == zzhuyin::KeyboardLayout::Standard);
    AddMenuCheckItem(pMenu, kMenuLayoutEten,     L"鍵盤：倚天",     cfg.engine.layout == zzhuyin::KeyboardLayout::Eten);
    AddMenuCheckItem(pMenu, kMenuLayoutIbm,      L"鍵盤：IBM",      cfg.engine.layout == zzhuyin::KeyboardLayout::Ibm);
    AddMenuCheckItem(pMenu, kMenuLayoutGinYieh,  L"鍵盤：精業",     cfg.engine.layout == zzhuyin::KeyboardLayout::GinYieh);
    AddMenuSeparator(pMenu);

    AddMenuCheckItem(pMenu, kMenuOrientVertical, L"候選窗：直排", cfg.engine.orientation == zzhuyin::CandidateOrientation::Vertical);
    AddMenuCheckItem(pMenu, kMenuOrientHoriz,    L"候選窗：橫排", cfg.engine.orientation == zzhuyin::CandidateOrientation::Horizontal);
    AddMenuSeparator(pMenu);

    AddMenuCheckItem(pMenu, kMenuToggleAssociate, L"相關字詞（Shift+1..9）", cfg.engine.prompt_associate_phrase);
    AddMenuCheckItem(pMenu, kMenuToggleBig5Only,  L"僅輸出 Big5 字元",      cfg.engine.charset_filter.output_big5_only);
    AddMenuCheckItem(pMenu, kMenuToggleExtA,      L"包含 CJK Ext-A 字元",   cfg.engine.charset_filter.include_ext_a);
    AddMenuCheckItem(pMenu, kMenuToggleCns,       L"允許 CNS 罕用破音",     cfg.engine.charset_filter.allow_cns);
    AddMenuCheckItem(pMenu, kMenuToggleBeep,      L"不合法注音提示音",      cfg.engine.enable_warning_beep);

    return S_OK;
}

STDMETHODIMP LangBarButton::OnMenuSelect(UINT wID) {
    if (wID == kMenuToggleNative) {
        if (cb_.toggle_native) cb_.toggle_native();
        NotifyUpdate();
        return S_OK;
    }
    if (wID == kMenuToggleFullShape) {
        if (cb_.toggle_full_shape) cb_.toggle_full_shape();
        NotifyUpdate();
        return S_OK;
    }

    if (!cb_.get_config || !cb_.update_config) {
        return S_OK;
    }

    PersistedConfig cfg = cb_.get_config();
    bool changed = true;

    switch (wID) {
        case kMenuLayoutStandard:  cfg.engine.layout = zzhuyin::KeyboardLayout::Standard; break;
        case kMenuLayoutEten:      cfg.engine.layout = zzhuyin::KeyboardLayout::Eten; break;
        case kMenuLayoutIbm:       cfg.engine.layout = zzhuyin::KeyboardLayout::Ibm; break;
        case kMenuLayoutGinYieh:   cfg.engine.layout = zzhuyin::KeyboardLayout::GinYieh; break;
        case kMenuOrientVertical:  cfg.engine.orientation = zzhuyin::CandidateOrientation::Vertical; break;
        case kMenuOrientHoriz:     cfg.engine.orientation = zzhuyin::CandidateOrientation::Horizontal; break;
        case kMenuToggleAssociate: cfg.engine.prompt_associate_phrase = !cfg.engine.prompt_associate_phrase; break;
        case kMenuToggleBig5Only:  cfg.engine.charset_filter.output_big5_only = !cfg.engine.charset_filter.output_big5_only; break;
        case kMenuToggleExtA:      cfg.engine.charset_filter.include_ext_a = !cfg.engine.charset_filter.include_ext_a; break;
        case kMenuToggleCns:       cfg.engine.charset_filter.allow_cns = !cfg.engine.charset_filter.allow_cns; break;
        case kMenuToggleBeep:      cfg.engine.enable_warning_beep = !cfg.engine.enable_warning_beep; break;
        default:                   changed = false; break;
    }

    if (changed) {
        cb_.update_config(cfg);
        NotifyUpdate();
    }
    return S_OK;
}

HICON LangBarButton::RenderModeIcon(bool is_native, bool is_full_shape) {
    constexpr int kSize = 32;

    BITMAPV5HEADER bi{};
    bi.bV5Size        = sizeof(bi);
    bi.bV5Width       = kSize;
    bi.bV5Height      = -kSize; // top-down
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    void* bits = nullptr;
    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);
    HBITMAP color_bmp = CreateDIBSection(
        screen_dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen_dc);

    if (!color_bmp || !bits) {
        if (color_bmp) DeleteObject(color_bmp);
        if (mem_dc) DeleteDC(mem_dc);
        return nullptr;
    }

    // Clear with solid badge background
    auto* pixels = static_cast<uint32_t*>(bits);
    const uint32_t bg_argb = is_native ? 0xFF0078D7u : 0xFF4A4A4Au;
    for (int i = 0; i < kSize * kSize; ++i) {
        pixels[i] = bg_argb;
    }

    HGDIOBJ old_bmp = SelectObject(mem_dc, color_bmp);

    LOGFONTW lf{};
    lf.lfHeight = -22;
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = NONANTIALIASED_QUALITY; // Keeps alpha channel clean for GDI text
    wcsncpy_s(lf.lfFaceName, L"Microsoft JhengHei UI", _TRUNCATE);
    HFONT font = CreateFontIndirectW(&lf);
    HGDIOBJ old_font = SelectObject(mem_dc, font);

    SetBkMode(mem_dc, TRANSPARENT);
    SetTextColor(mem_dc, RGB(255, 255, 255));

    RECT rc{0, 0, kSize, kSize};
    const wchar_t* glyph = is_native ? L"中" : L"英";
    DrawTextW(mem_dc, glyph, 1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // Small corner dot to indicate full-shape mode
    if (is_full_shape) {
        for (int y = kSize - 6; y < kSize - 2; ++y) {
            for (int x = kSize - 6; x < kSize - 2; ++x) {
                pixels[y * kSize + x] = 0xFFFFD700u;
            }
        }
    }

    // Ensure opaque alpha on all drawn pixels
    for (int i = 0; i < kSize * kSize; ++i) {
        pixels[i] |= 0xFF000000u;
    }

    SelectObject(mem_dc, old_font);
    DeleteObject(font);
    SelectObject(mem_dc, old_bmp);
    DeleteDC(mem_dc);

    HBITMAP mask_bmp = CreateBitmap(kSize, kSize, 1, 1, nullptr);
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmMask = mask_bmp;
    ii.hbmColor = color_bmp;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(mask_bmp);
    DeleteObject(color_bmp);
    return icon;
}

STDMETHODIMP LangBarButton::GetIcon(HICON* phIcon) {
    if (!phIcon) return E_INVALIDARG;
    const bool native = cb_.is_native ? cb_.is_native() : true;
    const bool full   = cb_.is_full_shape ? cb_.is_full_shape() : false;
    *phIcon = RenderModeIcon(native, full);
    return *phIcon ? S_OK : E_FAIL;
}

STDMETHODIMP LangBarButton::GetText(BSTR* pbstrText) {
    if (!pbstrText) return E_INVALIDARG;
    const bool native = cb_.is_native ? cb_.is_native() : true;
    *pbstrText = SysAllocString(native ? L"中" : L"英");
    return *pbstrText ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP LangBarButton::AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie) {
    if (!punk || !pdwCookie) return E_INVALIDARG;
    if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) return CONNECT_E_CANNOTCONNECT;
    if (sink_) return CONNECT_E_ADVISELIMIT;

    const HRESULT hr = punk->QueryInterface(IID_ITfLangBarItemSink, reinterpret_cast<void**>(&sink_));
    if (FAILED(hr)) return hr;

    sink_cookie_ = kLangBarSinkCookie;
    *pdwCookie = sink_cookie_;
    return S_OK;
}

STDMETHODIMP LangBarButton::UnadviseSink(DWORD dwCookie) {
    if (dwCookie != sink_cookie_ || !sink_) {
        return CONNECT_E_NOCONNECTION;
    }
    sink_->Release();
    sink_ = nullptr;
    sink_cookie_ = 0;
    return S_OK;
}

void LangBarButton::NotifyUpdate() {
    if (sink_) {
        sink_->OnUpdate(TF_LBI_ICON | TF_LBI_TEXT | TF_LBI_STATUS);
    }
}

} // namespace zzhuyin::tsf
