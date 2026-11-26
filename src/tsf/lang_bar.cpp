#include "tsf/lang_bar.h"

#include "tsf/guids.h"

#include <oleauto.h>
#include <olectl.h>
#include <cstring>

namespace zzhuyin::tsf {

extern void DllAddRef();
extern void DllRelease();

namespace {

constexpr DWORD kLangBarSinkCookie    = 0x5A5A0001;
constexpr UINT  kMenuOpenSettings     = 501;
constexpr UINT  kMenuOutputScript     = 502;
constexpr UINT  kMenuOutputTrad       = 503;
constexpr UINT  kMenuOutputSimplified = 504;

void HandleMenuCommand(UINT cmd, const LangBarCallbacks& cb) {
    if (cmd == kMenuOpenSettings) {
        if (cb.open_settings) {
            cb.open_settings();
        }
    } else if (cmd == kMenuOutputTrad || cmd == kMenuOutputSimplified) {
        if (cb.update_config) {
            PersistedConfig cfg = cb.get_config ? cb.get_config() : PersistedConfig{};
            cfg.engine.output_simplified = (cmd == kMenuOutputSimplified);
            cb.update_config(cfg);
        }
    }
}

void ShowRightClickPopup(
    POINT pt, const RECT* prcArea, const LangBarCallbacks& cb) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    const PersistedConfig cfg = cb.get_config ? cb.get_config() : PersistedConfig{};
    const bool is_simplified = cfg.engine.output_simplified;

    bool has_script_menu = false;
    if (HMENU script_submenu = CreatePopupMenu()) {
        AppendMenuW(
            script_submenu,
            MF_STRING | (!is_simplified ? MF_CHECKED : MF_UNCHECKED),
            kMenuOutputTrad,
            L"繁體");
        AppendMenuW(
            script_submenu,
            MF_STRING | (is_simplified ? MF_CHECKED : MF_UNCHECKED),
            kMenuOutputSimplified,
            L"簡體");
        if (AppendMenuW(
                menu,
                MF_POPUP,
                reinterpret_cast<UINT_PTR>(script_submenu),
                L"輸出字種")) {
            has_script_menu = true;
        } else {
            DestroyMenu(script_submenu);
        }
    }

    if (cb.open_settings) {
        if (has_script_menu) {
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        }
        AppendMenuW(menu, MF_STRING, kMenuOpenSettings, L"設定值");
    }

    // A temporary hidden window is required so TrackPopupMenuEx dismisses cleanly on click-away
    HWND helper = CreateWindowExW(
        WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);
    if (helper) {
        SetForegroundWindow(helper);
        if (pt.x == 0 && pt.y == 0) {
            GetCursorPos(&pt);
        }

        // Anchor menu cleanly above the taskbar / icon instead of overlapping the tray icon
        int x = prcArea ? prcArea->left : pt.x;
        int y = prcArea ? (prcArea->top - 6) : (pt.y - 24);
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(mon, &mi) && y > mi.rcWork.bottom - 4) {
            y = mi.rcWork.bottom - 4;
        }

        const UINT cmd = static_cast<UINT>(TrackPopupMenuEx(
            menu,
            TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
            x, y, helper, nullptr));
        PostMessageW(helper, WM_NULL, 0, 0);
        DestroyWindow(helper);

        HandleMenuCommand(cmd, cb);
    }
    DestroyMenu(menu);
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
    pInfo->dwStyle      = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
    pInfo->ulSort       = 0;
    wcsncpy_s(pInfo->szDescription, L"Z注音", _TRUNCATE);
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

STDMETHODIMP LangBarButton::OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) {
    if (click == TF_LBI_CLK_LEFT && cb_.toggle_native) {
        cb_.toggle_native();
        NotifyUpdate();
    } else if (click == TF_LBI_CLK_RIGHT) {
        ShowRightClickPopup(pt, prcArea, cb_);
    }
    return S_OK;
}

STDMETHODIMP LangBarButton::InitMenu(ITfMenu* pMenu) {
    if (!pMenu) return E_INVALIDARG;

    const PersistedConfig cfg = cb_.get_config ? cb_.get_config() : PersistedConfig{};
    const bool is_simplified = cfg.engine.output_simplified;

    constexpr wchar_t kOutputScriptText[] = L"輸出字種";
    ITfMenu* sub_menu = nullptr;
    if (SUCCEEDED(pMenu->AddMenuItem(
            kMenuOutputScript, TF_LBMENUF_SUBMENU, nullptr, nullptr,
            kOutputScriptText, static_cast<ULONG>(wcslen(kOutputScriptText)),
            &sub_menu)) && sub_menu) {
        constexpr wchar_t kTradText[] = L"繁體";
        constexpr wchar_t kSimpText[] = L"簡體";
        sub_menu->AddMenuItem(
            kMenuOutputTrad,
            !is_simplified ? TF_LBMENUF_CHECKED : 0,
            nullptr, nullptr,
            kTradText, static_cast<ULONG>(wcslen(kTradText)), nullptr);
        sub_menu->AddMenuItem(
            kMenuOutputSimplified,
            is_simplified ? TF_LBMENUF_CHECKED : 0,
            nullptr, nullptr,
            kSimpText, static_cast<ULONG>(wcslen(kSimpText)), nullptr);
        sub_menu->Release();
    }

    pMenu->AddMenuItem(0, TF_LBMENUF_SEPARATOR, nullptr, nullptr, nullptr, 0, nullptr);

    constexpr wchar_t kSettingsText[] = L"設定值";
    pMenu->AddMenuItem(
        kMenuOpenSettings, 0, nullptr, nullptr,
        kSettingsText, static_cast<ULONG>(wcslen(kSettingsText)), nullptr);
    return S_OK;
}

STDMETHODIMP LangBarButton::OnMenuSelect(UINT wID) {
    HandleMenuCommand(wID, cb_);
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

    // Classic Win7 framed keycap tile silhouette with modern Teal/Ivory (Chinese) & Slate (English) palette
    auto* pixels = static_cast<uint32_t*>(bits);
    const uint32_t outer_border = is_native ? 0xFF134E4Au : 0xFF334155u; // Deep Teal / Dark Slate
    const uint32_t inner_rim    = is_native ? 0xFF99F6E4u : 0xFFCBD5E1u; // Soft Mint / Cool Rim
    const uint32_t inner_bg     = is_native ? 0xFFF0FDFAu : 0xFFF1F5F9u; // Crisp Ice-Ivory / Light Slate

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            uint32_t col = inner_bg;
            if (x < 2 || x >= kSize - 2 || y < 2 || y >= kSize - 2) {
                col = outer_border;
            } else if (x == 2 || x == kSize - 3 || y == 2 || y == kSize - 3) {
                col = inner_rim;
            }
            pixels[y * kSize + x] = col;
        }
    }

    HGDIOBJ old_bmp = SelectObject(mem_dc, color_bmp);

    LOGFONTW lf{};
    lf.lfHeight = -23;
    lf.lfWeight = FW_EXTRABOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = NONANTIALIASED_QUALITY;
    wcsncpy_s(lf.lfFaceName, L"Microsoft JhengHei UI", _TRUNCATE);
    HFONT font = CreateFontIndirectW(&lf);
    HGDIOBJ old_font = SelectObject(mem_dc, font);

    SetBkMode(mem_dc, TRANSPARENT);

    const wchar_t* glyph = is_native ? L"ㄅ" : L"英";
    RECT rc{0, -1, kSize, kSize - 1};

    // Subtle bright halo behind the glyph (matching the classic Win7 ㄅ icon aesthetic)
    SetTextColor(mem_dc, RGB(255, 255, 255));
    const POINT offsets[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (const auto& off : offsets) {
        RECT hrc{rc.left + off.x, rc.top + off.y, rc.right + off.x, rc.bottom + off.y};
        DrawTextW(mem_dc, glyph, 1, &hrc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }

    // Main bold glyph: Vibrant Deep Teal for ㄅ, Charcoal Slate for 英
    SetTextColor(mem_dc, is_native ? RGB(15, 118, 110) : RGB(51, 65, 85));
    DrawTextW(mem_dc, glyph, 1, &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // Small warm amber dot at bottom-right inside the tile when Full-Shape is active
    if (is_full_shape) {
        for (int y = kSize - 7; y < kSize - 3; ++y) {
            for (int x = kSize - 7; x < kSize - 3; ++x) {
                pixels[y * kSize + x] = 0xFFF59E0Bu;
            }
        }
    }

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
    *pbstrText = SysAllocString(native ? L"ㄅ" : L"英");
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
