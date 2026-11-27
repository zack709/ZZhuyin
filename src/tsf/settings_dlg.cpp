#include "tsf/settings_dlg.h"

#include "core/key_mapper.h"
#include "core/types.h"
#include "tsf/lang_bar.h"

#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace zzhuyin::tsf {

extern void DllAddRef();
extern void DllRelease();

namespace {

constexpr wchar_t kMainWndClass[]    = L"ZZhuyinSettingsMainWnd";
constexpr wchar_t kCharsetWndClass[] = L"ZZhuyinCharsetDlgWnd";
constexpr wchar_t kKbdPreviewClass[] = L"ZZhuyinKeyboardPreview";

std::atomic<HWND> g_activeSettingsWnd{nullptr};

enum ControlId : int {
    kIdTab = 100,
    kIdBtnOk = 101,
    kIdBtnCancel = 102,

    // General tab
    kIdCmbDefaultMode = 201,
    kIdCmbFontSize = 202,
    kIdCmbOrientation = 203,
    kIdCmbTheme = 204,
    kIdChkAssociate = 205,
    kIdBtnCharset = 206,
    kIdRadTraditional = 207,
    kIdRadSimplified = 208,

    // Advanced tab
    kIdBtnResetDefaults = 301,
    kIdListAdvanced = 302,
    kIdLblAdvDesc = 303,
    kIdRadOpt0 = 304,
    kIdRadOpt1 = 305,

    // Keyboard tab
    kIdCmbLayout = 401,
    kIdKbdPreview = 402,

    // Charset sub-dialog
    kIdRadBig5 = 501,
    kIdRadUnicode = 502,
    kIdChkExtA = 503,
    kIdChkExtB = 504,
    kIdChkHkscs = 505,
    kIdChkCns = 506,
    kIdCharsetOk = 507,
    kIdCharsetCancel = 508,

    // Profile (設定檔) tab
    kIdBtnExportProfile = 601,
    kIdBtnImportProfile = 602,
};

struct FontSizeChoice {
    const wchar_t* label;
    int pt;
};

constexpr FontSizeChoice kFontSizeChoices[] = {
    {L"系統預設(100%)", 12},
    {L"較大(125%)",     15},
    {L"大(150%)",       18},
    {L"超大(200%)",     24},
};

struct AdvItemMeta {
    const wchar_t* name;
    const wchar_t* desc;
    const wchar_t* opt0_label;
    const wchar_t* opt1_label;
};

constexpr AdvItemMeta kAdvItems[] = {
    {
        L"使用左側 Shift 鍵",
        L"決定鍵盤左邊的 SHIFT 按鍵是要提供切換(中文/英數)輸入模式，或是完全不提供任何特殊功能。",
        L"切換輸入模式",
        L"(無)",
    },
    {
        L"使用右側 Shift 鍵",
        L"決定鍵盤右邊的 SHIFT 按鍵是要提供切換(中文/英數)輸入模式，或是完全不提供任何特殊功能。",
        L"切換輸入模式",
        L"(無)",
    },
    {
        L"標點符號輸入",
        L"決定是否啟用 Ctrl+標點快捷鍵（如 Ctrl+, 輸出全形逗號）與全形標點符號輸入功能。",
        L"使用",
        L"不使用",
    },
    {
        L"警告嗶聲提示",
        L"決定當輸入不合法的注音組合時，是否發出系統警告提示音。",
        L"使用",
        L"不使用",
    },
};

struct ThemeColors {
    COLORREF win_bg;
    COLORREF page_bg;
    COLORREF tab_inactive_bg;
    COLORREF border;
    COLORREF text;
    COLORREF key_bg;
    COLORREF key_border;
};

ThemeColors GetThemeColors(bool dark) {
    if (dark) {
        return ThemeColors{
            RGB(32, 32, 32),   // win_bg
            RGB(43, 43, 43),   // page_bg
            RGB(36, 36, 36),   // tab_inactive_bg
            RGB(74, 74, 74),   // border
            RGB(240, 240, 240),// text
            RGB(58, 58, 58),   // key_bg
            RGB(96, 96, 96),   // key_border
        };
    }
    return ThemeColors{
        RGB(243, 243, 243), // win_bg
        RGB(255, 255, 255), // page_bg
        RGB(240, 240, 240), // tab_inactive_bg
        RGB(214, 214, 214), // border
        RGB(24, 24, 24),    // text
        RGB(242, 242, 242), // key_bg
        RGB(172, 172, 172), // key_border
    };
}

void ApplyDwmDarkTitleBar(HWND hwnd, bool dark) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    auto fn = reinterpret_cast<DwmSetWindowAttributeFn>(
        GetProcAddress(dwm, "DwmSetWindowAttribute"));
    if (fn) {
        const BOOL val = dark ? TRUE : FALSE;
        // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (Win10 20H1+ / Win11)
        if (FAILED(fn(hwnd, 20, &val, sizeof(val)))) {
            fn(hwnd, 19, &val, sizeof(val));
        }
    }
    FreeLibrary(dwm);
}

void ApplyUxTheme(HWND hwnd, const wchar_t* sub_app, const wchar_t* sub_id = nullptr) {
    if (!hwnd) return;
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;
    using SetWindowThemeFn = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
    auto fn = reinterpret_cast<SetWindowThemeFn>(GetProcAddress(ux, "SetWindowTheme"));
    if (fn) {
        fn(hwnd, sub_app, sub_id);
    }
    FreeLibrary(ux);
}

struct SettingsApp {
    HINSTANCE hInst = nullptr;
    HWND hwnd = nullptr;
    HFONT font = nullptr;
    HFONT kbd_font = nullptr;
    UINT dpi = 96;

    HBRUSH win_brush = nullptr;
    HBRUSH page_brush = nullptr;

    PersistedConfig cfg{};
    std::function<void(const PersistedConfig&)> on_saved;

    HWND tab = nullptr;
    HWND btn_ok = nullptr;
    HWND btn_cancel = nullptr;

    std::vector<HWND> tab0_ctrls;
    HWND cmb_default_mode = nullptr;
    HWND cmb_font_size = nullptr;
    HWND cmb_orientation = nullptr;
    HWND cmb_theme = nullptr;
    HWND chk_associate = nullptr;
    HWND rad_traditional = nullptr;
    HWND rad_simplified = nullptr;
    HWND btn_charset = nullptr;

    std::vector<HWND> tab1_ctrls;
    HWND btn_reset = nullptr;
    HWND list_adv = nullptr;
    HWND lbl_adv_desc = nullptr;
    HWND rad_opt0 = nullptr;
    HWND rad_opt1 = nullptr;
    int selected_adv_row = 0;

    std::vector<HWND> tab2_ctrls;
    HWND cmb_layout = nullptr;
    HWND kbd_preview = nullptr;

    std::vector<HWND> tab3_ctrls;
    HWND btn_export = nullptr;
    HWND btn_import = nullptr;

    int S(int px) const { return MulDiv(px, static_cast<int>(dpi), 96); }
};

UINT QueryDpi(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using Fn = UINT(WINAPI*)(HWND);
        auto fn = reinterpret_cast<Fn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (fn && hwnd) {
            UINT d = fn(hwnd);
            if (d > 0) return d;
        }
    }
    return 96;
}

HFONT CreateUiFont(UINT dpi, int pt, int weight = FW_NORMAL) {
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(pt, static_cast<int>(dpi), 72);
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcsncpy_s(lf.lfFaceName, L"Microsoft JhengHei UI", _TRUNCATE);
    return CreateFontIndirectW(&lf);
}

void ApplyFont(HWND ctrl, HFONT font) {
    if (ctrl && font) {
        SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

bool GetAdvValue(const PersistedConfig& cfg, int row) {
    switch (row) {
        case 0: return cfg.engine.left_shift_toggle_mode;
        case 1: return cfg.engine.right_shift_toggle_mode;
        case 2: return cfg.engine.enable_punctuation_input;
        case 3: return cfg.engine.enable_warning_beep;
        default: return false;
    }
}

void SetAdvValue(PersistedConfig& cfg, int row, bool val) {
    switch (row) {
        case 0: cfg.engine.left_shift_toggle_mode = val; break;
        case 1: cfg.engine.right_shift_toggle_mode = val; break;
        case 2: cfg.engine.enable_punctuation_input = val; break;
        case 3: cfg.engine.enable_warning_beep = val; break;
        default: break;
    }
}

std::wstring LookupKeycapGlyph(zzhuyin::KeyboardLayout layout, char ascii_key) {
    auto mapped = zzhuyin::KeyMapper::MapBopomofo(layout, static_cast<char16_t>(ascii_key));
    if (!mapped) return {};
    char16_t ch = 0;
    switch (mapped->slot) {
        case zzhuyin::BopomofoSlot::Initial:
            if (mapped->value < zzhuyin::kInitialChars.size()) ch = zzhuyin::kInitialChars[mapped->value];
            break;
        case zzhuyin::BopomofoSlot::Medial:
            if (mapped->value < zzhuyin::kMedialChars.size()) ch = zzhuyin::kMedialChars[mapped->value];
            break;
        case zzhuyin::BopomofoSlot::Final:
            if (mapped->value < zzhuyin::kFinalChars.size()) ch = zzhuyin::kFinalChars[mapped->value];
            break;
        case zzhuyin::BopomofoSlot::Tone:
            if (mapped->value > 1 && mapped->value < zzhuyin::kToneChars.size()) ch = zzhuyin::kToneChars[mapped->value];
            break;
    }
    if (ch == 0) return {};
    return std::wstring(1, static_cast<wchar_t>(ch));
}

void RebuildThemeBrushes(SettingsApp& app) {
    if (app.win_brush) { DeleteObject(app.win_brush); app.win_brush = nullptr; }
    if (app.page_brush) { DeleteObject(app.page_brush); app.page_brush = nullptr; }
    const ThemeColors tc = GetThemeColors(app.cfg.ui.dark_theme);
    app.win_brush = CreateSolidBrush(tc.win_bg);
    app.page_brush = CreateSolidBrush(tc.page_bg);
}

void ApplyThemeToMainWindow(SettingsApp& app) {
    const bool dark = app.cfg.ui.dark_theme;
    const ThemeColors tc = GetThemeColors(dark);
    RebuildThemeBrushes(app);

    ApplyDwmDarkTitleBar(app.hwnd, dark);

    // Push buttons & ComboBoxes
    const wchar_t* btn_theme = dark ? L"DarkMode_Explorer" : L"Explorer";
    const wchar_t* cmb_theme = dark ? L"DarkMode_CFD" : L"Explorer";

    for (HWND h : {app.btn_ok, app.btn_cancel, app.btn_charset, app.btn_reset, app.btn_export, app.btn_import}) {
        ApplyUxTheme(h, btn_theme);
    }
    for (HWND h : {app.cmb_default_mode, app.cmb_font_size, app.cmb_orientation, app.cmb_theme, app.cmb_layout}) {
        ApplyUxTheme(h, cmb_theme);
    }

    // Checkboxes & Radio buttons: disable visual style in Dark mode so WM_CTLCOLORSTATIC text color applies cleanly
    for (HWND h : {app.chk_associate, app.rad_traditional, app.rad_simplified, app.rad_opt0, app.rad_opt1}) {
        ApplyUxTheme(h, dark ? L"" : nullptr, dark ? L"" : nullptr);
    }

    // ListView colors
    if (app.list_adv) {
        ApplyUxTheme(app.list_adv, btn_theme);
        ListView_SetBkColor(app.list_adv, tc.page_bg);
        ListView_SetTextBkColor(app.list_adv, tc.page_bg);
        ListView_SetTextColor(app.list_adv, tc.text);
    }

    RedrawWindow(app.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

// Custom-painted TabControl subclass so both Light & Dark mode tab body & borders are seamless
LRESULT CALLBACK TabSubclassProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR /*id*/, DWORD_PTR ref_data) {
    auto* app = reinterpret_cast<SettingsApp*>(ref_data);
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_PAINT && app) {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc{};
        GetClientRect(hwnd, &rc);
        const ThemeColors tc = GetThemeColors(app->cfg.ui.dark_theme);

        // Fill top strip with outer window bg
        FillRect(hdc, &rc, app->win_brush);

        // Compute page display rect
        RECT page_rc = rc;
        TabCtrl_AdjustRect(hwnd, FALSE, &page_rc);
        page_rc.left   = rc.left;
        page_rc.right  = rc.right;
        page_rc.bottom = rc.bottom;
        page_rc.top    = std::max(0L, page_rc.top - 2);

        // Fill page body & border
        FillRect(hdc, &page_rc, app->page_brush);
        HPEN border_pen = CreatePen(PS_SOLID, 1, tc.border);
        HGDIOBJ old_pen = SelectObject(hdc, border_pen);
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, page_rc.left, page_rc.top, page_rc.right, page_rc.bottom);

        // Draw each tab button
        const int cur_sel = TabCtrl_GetCurSel(hwnd);
        const int count = TabCtrl_GetItemCount(hwnd);
        HGDIOBJ old_font = app->font ? SelectObject(hdc, app->font) : nullptr;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, tc.text);

        HBRUSH inactive_br = CreateSolidBrush(tc.tab_inactive_bg);
        for (int i = 0; i < count; ++i) {
            RECT item_rc{};
            TabCtrl_GetItemRect(hwnd, i, &item_rc);
            const bool selected = (i == cur_sel);
            if (selected) {
                item_rc.bottom = page_rc.top + 1;
                FillRect(hdc, &item_rc, app->page_brush);
                // Top, left, right border (bottom stays open into page)
                MoveToEx(hdc, item_rc.left, item_rc.bottom - 1, nullptr);
                LineTo(hdc, item_rc.left, item_rc.top);
                LineTo(hdc, item_rc.right - 1, item_rc.top);
                LineTo(hdc, item_rc.right - 1, item_rc.bottom);
            } else {
                item_rc.top += 2;
                item_rc.bottom = page_rc.top;
                FillRect(hdc, &item_rc, inactive_br);
                MoveToEx(hdc, item_rc.left, item_rc.bottom, nullptr);
                LineTo(hdc, item_rc.left, item_rc.top);
                LineTo(hdc, item_rc.right - 1, item_rc.top);
                LineTo(hdc, item_rc.right - 1, item_rc.bottom);
            }

            wchar_t text[32] = {};
            TCITEMW ti{};
            ti.mask = TCIF_TEXT;
            ti.pszText = text;
            ti.cchTextMax = 32;
            TabCtrl_GetItem(hwnd, i, &ti);
            DrawTextW(hdc, text, -1, &item_rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        }

        DeleteObject(inactive_br);
        if (old_font) SelectObject(hdc, old_font);
        SelectObject(hdc, old_brush);
        SelectObject(hdc, old_pen);
        DeleteObject(border_pen);

        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK KeyboardPreviewProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc{};
        GetClientRect(hwnd, &rc);
        auto* app = reinterpret_cast<SettingsApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        const auto layout = app ? app->cfg.engine.layout : zzhuyin::KeyboardLayout::Standard;
        const bool dark   = app ? app->cfg.ui.dark_theme : false;
        const ThemeColors tc = GetThemeColors(dark);

        HBRUSH bg = CreateSolidBrush(tc.page_bg);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        HPEN frame_pen = CreatePen(PS_SOLID, 1, tc.border);
        HGDIOBJ prev_pen = SelectObject(hdc, frame_pen);
        HGDIOBJ prev_br = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, prev_br);
        SelectObject(hdc, prev_pen);
        DeleteObject(frame_pen);

        HGDIOBJ old_font = (app && app->kbd_font) ? SelectObject(hdc, app->kbd_font) : nullptr;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, tc.text);

        const int pad = app ? app->S(6) : 6;
        const int gap = app ? app->S(3) : 3;
        const int avail_w = (rc.right - rc.left) - pad * 2;
        const int avail_h = (rc.bottom - rc.top) - pad * 2;
        const int row_h = (avail_h - gap * 4) / 5;

        struct KeySpec {
            const wchar_t* fixed_label;
            char ascii;
            double units;
        };

        const std::vector<std::vector<KeySpec>> rows = {
            {
                {nullptr, '`', 1.0}, {nullptr, '1', 1.0}, {nullptr, '2', 1.0}, {nullptr, '3', 1.0},
                {nullptr, '4', 1.0}, {nullptr, '5', 1.0}, {nullptr, '6', 1.0}, {nullptr, '7', 1.0},
                {nullptr, '8', 1.0}, {nullptr, '9', 1.0}, {nullptr, '0', 1.0}, {nullptr, '-', 1.0},
                {nullptr, '=', 1.0}, {nullptr, '\\', 1.0}, {L"<-", 0, 1.0},
            },
            {
                {L"Tab", 0, 1.5}, {nullptr, 'q', 1.0}, {nullptr, 'w', 1.0}, {nullptr, 'e', 1.0},
                {nullptr, 'r', 1.0}, {nullptr, 't', 1.0}, {nullptr, 'y', 1.0}, {nullptr, 'u', 1.0},
                {nullptr, 'i', 1.0}, {nullptr, 'o', 1.0}, {nullptr, 'p', 1.0}, {nullptr, '[', 1.0},
                {nullptr, ']', 1.0}, {L"Enter", 0, 1.5},
            },
            {
                {L"Caps", 0, 1.8}, {nullptr, 'a', 1.0}, {nullptr, 's', 1.0}, {nullptr, 'd', 1.0},
                {nullptr, 'f', 1.0}, {nullptr, 'g', 1.0}, {nullptr, 'h', 1.0}, {nullptr, 'j', 1.0},
                {nullptr, 'k', 1.0}, {nullptr, 'l', 1.0}, {nullptr, ';', 1.0}, {nullptr, '\'', 1.0},
                {L"", 0, 2.2},
            },
            {
                {L"Shift", 0, 2.3}, {nullptr, 'z', 1.0}, {nullptr, 'x', 1.0}, {nullptr, 'c', 1.0},
                {nullptr, 'v', 1.0}, {nullptr, 'b', 1.0}, {nullptr, 'n', 1.0}, {nullptr, 'm', 1.0},
                {nullptr, ',', 1.0}, {nullptr, '.', 1.0}, {nullptr, '/', 1.0}, {L"Shift", 0, 2.7},
            },
            {
                {L"Ctrl", 0, 1.5}, {L"Esc", 0, 1.5}, {L"Alt", 0, 1.5},
                {L"", 0, 6.0},
                {L"Alt", 0, 1.5}, {L"", 0, 1.5}, {L"Ctrl", 0, 1.5},
            },
        };

        HBRUSH key_bg = CreateSolidBrush(tc.key_bg);
        HPEN key_pen = CreatePen(PS_SOLID, 1, tc.key_border);
        HGDIOBJ old_pen = SelectObject(hdc, key_pen);
        HGDIOBJ old_brush = SelectObject(hdc, key_bg);

        for (size_t r = 0; r < rows.size(); ++r) {
            const auto& row = rows[r];
            double total_units = 0.0;
            for (const auto& k : row) total_units += k.units;
            const double unit_w =
                static_cast<double>(avail_w - gap * static_cast<int>(row.size() - 1)) / total_units;

            double x = static_cast<double>(pad);
            const int y = pad + static_cast<int>(r) * (row_h + gap);

            for (size_t c = 0; c < row.size(); ++c) {
                const auto& k = row[c];
                const int x1 = static_cast<int>(x);
                const int x2 = (c + 1 == row.size())
                    ? (rc.right - pad)
                    : static_cast<int>(x + k.units * unit_w);
                x += k.units * unit_w + gap;

                if (r == 4 && c == 5) continue;
                if (r == 2 && c == row.size() - 1) continue;

                RECT krc{x1, y, x2, y + row_h};
                if (r == 1 && c == row.size() - 1) {
                    krc.bottom = y + row_h * 2 + gap;
                }

                RoundRect(hdc, krc.left, krc.top, krc.right, krc.bottom, 4, 4);

                std::wstring label;
                if (k.fixed_label) {
                    label = k.fixed_label;
                } else if (k.ascii != 0) {
                    label = LookupKeycapGlyph(layout, k.ascii);
                }

                if (!label.empty()) {
                    DrawTextW(hdc, label.c_str(), static_cast<int>(label.size()), &krc,
                              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
                }
            }
        }

        SelectObject(hdc, old_brush);
        SelectObject(hdc, old_pen);
        DeleteObject(key_pen);
        DeleteObject(key_bg);
        if (old_font) SelectObject(hdc, old_font);

        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

struct CharsetDialogState {
    zzhuyin::CharsetFilterConfig filter{};
    bool dark_theme = false;
    bool accepted = false;
    bool done = false;
    HBRUSH bg_brush = nullptr;
    HWND rad_big5 = nullptr;
    HWND rad_unicode = nullptr;
    HWND chk_exta = nullptr;
    HWND chk_extb = nullptr;
    HWND chk_hkscs = nullptr;
    HWND chk_cns = nullptr;
};

void UpdateCharsetSubControls(CharsetDialogState* st) {
    if (!st) return;
    const bool is_unicode = (SendMessageW(st->rad_unicode, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(st->chk_exta, is_unicode ? TRUE : FALSE);
    EnableWindow(st->chk_extb, is_unicode ? TRUE : FALSE);
}

LRESULT CALLBACK CharsetDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<CharsetDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = static_cast<CharsetDialogState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
    }

    switch (msg) {
        case WM_ERASEBKGND:
            if (st && st->bg_brush) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wParam), &rc, st->bg_brush);
                return 1;
            }
            break;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            if (st && st->bg_brush) {
                const ThemeColors tc = GetThemeColors(st->dark_theme);
                HDC hdc = reinterpret_cast<HDC>(wParam);
                SetTextColor(hdc, tc.text);
                SetBkColor(hdc, tc.win_bg);
                return reinterpret_cast<LRESULT>(st->bg_brush);
            }
            break;

        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            const int code = HIWORD(wParam);
            if ((id == kIdRadBig5 || id == kIdRadUnicode) && code == BN_CLICKED) {
                UpdateCharsetSubControls(st);
                return 0;
            }
            if (id == kIdCharsetOk && st) {
                st->filter.output_big5_only = (SendMessageW(st->rad_big5, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->filter.include_ext_a    = !st->filter.output_big5_only &&
                    (SendMessageW(st->chk_exta, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->filter.include_ext_b    = !st->filter.output_big5_only &&
                    (SendMessageW(st->chk_extb, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->filter.include_hkscs    = (SendMessageW(st->chk_hkscs, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->filter.allow_cns        = (SendMessageW(st->chk_cns, BM_GETCHECK, 0, 0) == BST_CHECKED);
                st->accepted = true;
                st->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == kIdCharsetCancel && st) {
                st->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            if (st) st->done = true;
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowCharsetDialog(SettingsApp& app) {
    CharsetDialogState st{};
    st.filter = app.cfg.engine.charset_filter;
    st.dark_theme = app.cfg.ui.dark_theme;
    const ThemeColors tc = GetThemeColors(st.dark_theme);
    st.bg_brush = CreateSolidBrush(tc.win_bg);

    const int w = app.S(486);
    const int h = app.S(284);
    RECT parent_rc{};
    GetWindowRect(app.hwnd, &parent_rc);
    const int x = parent_rc.left + ((parent_rc.right - parent_rc.left) - w) / 2;
    const int y = parent_rc.top + ((parent_rc.bottom - parent_rc.top) - h) / 2;

    HWND dlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kCharsetWndClass,
        L"字元集設定",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h,
        app.hwnd, nullptr, app.hInst, &st);
    if (!dlg) {
        DeleteObject(st.bg_brush);
        return;
    }

    ApplyDwmDarkTitleBar(dlg, st.dark_theme);

    HWND grp = CreateWindowExW(
        0, L"BUTTON", L"設定 :",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        app.S(14), app.S(12), app.S(444), app.S(184),
        dlg, nullptr, app.hInst, nullptr);
    ApplyFont(grp, app.font);
    if (st.dark_theme) ApplyUxTheme(grp, L"", L"");

    st.rad_big5 = CreateWindowExW(
        0, L"BUTTON", L"BIG5 字元 (I)",
        WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP | BS_AUTORADIOBUTTON,
        app.S(26), app.S(36), app.S(220), app.S(22),
        dlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRadBig5)), app.hInst, nullptr);
    st.rad_unicode = CreateWindowExW(
        0, L"BUTTON", L"Unicode 字元 (U)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
        app.S(26), app.S(60), app.S(220), app.S(22),
        dlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRadUnicode)), app.hInst, nullptr);
    ApplyFont(st.rad_big5, app.font);
    ApplyFont(st.rad_unicode, app.font);
    if (st.dark_theme) {
        ApplyUxTheme(st.rad_big5, L"", L"");
        ApplyUxTheme(st.rad_unicode, L"", L"");
    }

    SendMessageW(st.rad_big5, BM_SETCHECK, st.filter.output_big5_only ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(st.rad_unicode, BM_SETCHECK, st.filter.output_big5_only ? BST_UNCHECKED : BST_CHECKED, 0);

    auto add_chk = [&](int id, const wchar_t* text, int xx, int yy, int ww, bool checked) -> HWND {
        HWND c = CreateWindowExW(
            0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            app.S(xx), app.S(yy), app.S(ww), app.S(22),
            dlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), app.hInst, nullptr);
        ApplyFont(c, app.font);
        if (st.dark_theme) ApplyUxTheme(c, L"", L"");
        SendMessageW(c, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
        return c;
    };

    st.chk_exta  = add_chk(kIdChkExtA,  L"包含中日韓統一表意文字擴充 A 字元 (X)", 56,  86, 380, st.filter.include_ext_a);
    st.chk_extb  = add_chk(kIdChkExtB,  L"包含中日韓統一表意文字擴充 B 字元 (B)", 56, 110, 380, st.filter.include_ext_b);
    st.chk_hkscs = add_chk(kIdChkHkscs, L"包含香港增補字符集字元(H)",               26, 138, 380, st.filter.include_hkscs);
    st.chk_cns   = add_chk(kIdChkCns,   L"包含更多中文標準交換碼全字庫 (CNS11643) 所定義的字元 (C)", 26, 166, 424, st.filter.allow_cns);

    UpdateCharsetSubControls(&st);

    HWND ok = CreateWindowExW(
        0, L"BUTTON", L"確定",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        app.S(282), app.S(208), app.S(82), app.S(26),
        dlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCharsetOk)), app.hInst, nullptr);
    HWND cancel = CreateWindowExW(
        0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        app.S(374), app.S(208), app.S(82), app.S(26),
        dlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCharsetCancel)), app.hInst, nullptr);
    ApplyFont(ok, app.font);
    ApplyFont(cancel, app.font);
    if (st.dark_theme) {
        ApplyUxTheme(ok, L"DarkMode_Explorer");
        ApplyUxTheme(cancel, L"DarkMode_Explorer");
    }

    EnableWindow(app.hwnd, FALSE);
    MSG m{};
    while (!st.done && GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dlg, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    EnableWindow(app.hwnd, TRUE);
    SetForegroundWindow(app.hwnd);
    DeleteObject(st.bg_brush);

    if (st.accepted) {
        app.cfg.engine.charset_filter = st.filter;
    }
}

void SyncAdvancedDetail(SettingsApp& app) {
    const int row = std::clamp(app.selected_adv_row, 0, 3);
    const auto& meta = kAdvItems[row];
    const bool val = GetAdvValue(app.cfg, row);

    SetWindowTextW(app.lbl_adv_desc, meta.desc);
    SetWindowTextW(app.rad_opt0, meta.opt0_label);
    SetWindowTextW(app.rad_opt1, meta.opt1_label);
    SendMessageW(app.rad_opt0, BM_SETCHECK, val ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(app.rad_opt1, BM_SETCHECK, val ? BST_UNCHECKED : BST_CHECKED, 0);
}

void RefreshAdvancedList(SettingsApp& app) {
    for (int i = 0; i < 4; ++i) {
        const bool val = GetAdvValue(app.cfg, i);
        const wchar_t* text = val ? kAdvItems[i].opt0_label : kAdvItems[i].opt1_label;
        ListView_SetItemText(app.list_adv, i, 1, const_cast<LPWSTR>(text));
    }
}

void LoadConfigIntoUi(SettingsApp& app) {
    SendMessageW(app.cmb_default_mode, CB_SETCURSEL, app.cfg.engine.default_native_mode ? 0 : 1, 0);

    int font_idx = 0;
    for (int i = 0; i < 4; ++i) {
        if (kFontSizeChoices[i].pt == app.cfg.ui.font_size_pt) {
            font_idx = i;
            break;
        }
    }
    SendMessageW(app.cmb_font_size, CB_SETCURSEL, font_idx, 0);
    SendMessageW(app.cmb_orientation, CB_SETCURSEL,
                 app.cfg.engine.orientation == zzhuyin::CandidateOrientation::Vertical ? 0 : 1, 0);
    SendMessageW(app.cmb_theme, CB_SETCURSEL, app.cfg.ui.dark_theme ? 1 : 0, 0);
    SendMessageW(app.chk_associate, BM_SETCHECK,
                 app.cfg.engine.prompt_associate_phrase ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(app.rad_traditional, BM_SETCHECK,
                 app.cfg.engine.output_simplified ? BST_UNCHECKED : BST_CHECKED, 0);
    SendMessageW(app.rad_simplified, BM_SETCHECK,
                 app.cfg.engine.output_simplified ? BST_CHECKED : BST_UNCHECKED, 0);

    RefreshAdvancedList(app);
    SyncAdvancedDetail(app);

    int layout_idx = std::clamp(static_cast<int>(app.cfg.engine.layout) - 1, 0, 3);
    SendMessageW(app.cmb_layout, CB_SETCURSEL, layout_idx, 0);
    ApplyThemeToMainWindow(app);
}

void ShowTabPage(SettingsApp& app, int tab_idx) {
    auto set_vis = [](const std::vector<HWND>& ctrls, bool show) {
        for (HWND h : ctrls) ShowWindow(h, show ? SW_SHOW : SW_HIDE);
    };
    set_vis(app.tab0_ctrls, tab_idx == 0);
    set_vis(app.tab1_ctrls, tab_idx == 1);
    set_vis(app.tab2_ctrls, tab_idx == 2);
    set_vis(app.tab3_ctrls, tab_idx == 3);
}

void BuildControls(SettingsApp& app) {
    HWND parent = app.hwnd;

    app.tab = CreateWindowExW(
        0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
        app.S(12), app.S(10), app.S(464), app.S(436),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTab)), app.hInst, nullptr);
    ApplyFont(app.tab, app.font);
    SetWindowSubclass(app.tab, TabSubclassProc, 1, reinterpret_cast<DWORD_PTR>(&app));

    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    ti.pszText = const_cast<LPWSTR>(L"一般"); TabCtrl_InsertItem(app.tab, 0, &ti);
    ti.pszText = const_cast<LPWSTR>(L"進階"); TabCtrl_InsertItem(app.tab, 1, &ti);
    ti.pszText = const_cast<LPWSTR>(L"鍵盤"); TabCtrl_InsertItem(app.tab, 2, &ti);
    ti.pszText = const_cast<LPWSTR>(L"設定檔"); TabCtrl_InsertItem(app.tab, 3, &ti);

    app.btn_ok = CreateWindowExW(
        0, L"BUTTON", L"確認",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        app.S(310), app.S(456), app.S(78), app.S(26),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBtnOk)), app.hInst, nullptr);
    app.btn_cancel = CreateWindowExW(
        0, L"BUTTON", L"取消",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        app.S(396), app.S(456), app.S(78), app.S(26),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBtnCancel)), app.hInst, nullptr);
    ApplyFont(app.btn_ok, app.font);
    ApplyFont(app.btn_cancel, app.font);

    auto add_label = [&](std::vector<HWND>& bucket, const wchar_t* text, int x, int y, int w, int h, int id = 0) {
        HWND lbl = CreateWindowExW(
            0, L"STATIC", text,
            WS_CHILD | SS_LEFT,
            app.S(x), app.S(y), app.S(w), app.S(h),
            parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), app.hInst, nullptr);
        ApplyFont(lbl, app.font);
        bucket.push_back(lbl);
        return lbl;
    };

    auto add_sep = [&](std::vector<HWND>& bucket, int x, int y, int w) {
        HWND sep = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | SS_ETCHEDHORZ,
            app.S(x), app.S(y), app.S(w), app.S(2),
            parent, nullptr, app.hInst, nullptr);
        bucket.push_back(sep);
    };

    // Tab 0: 一般
    add_label(app.tab0_ctrls, L"一般設定:", 26, 44, 70, 18);
    add_sep(app.tab0_ctrls, 96, 52, 360);

    add_label(app.tab0_ctrls, L"選擇應用程式開啟時，輸入法的預設輸入模式。", 38, 64, 400, 18);
    add_label(app.tab0_ctrls, L"預設輸入模式(D):", 56, 88, 110, 18);
    app.cmb_default_mode = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
        app.S(172), app.S(84), app.S(160), app.S(120),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCmbDefaultMode)), app.hInst, nullptr);
    ApplyFont(app.cmb_default_mode, app.font);
    SendMessageW(app.cmb_default_mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"中文模式"));
    SendMessageW(app.cmb_default_mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"英文模式"));
    app.tab0_ctrls.push_back(app.cmb_default_mode);

    add_label(app.tab0_ctrls, L"變更候選字窗的字型大小。", 38, 114, 400, 18);
    add_label(app.tab0_ctrls, L"字型大小設定(F):", 56, 138, 110, 18);
    app.cmb_font_size = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
        app.S(172), app.S(134), app.S(160), app.S(140),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCmbFontSize)), app.hInst, nullptr);
    ApplyFont(app.cmb_font_size, app.font);
    for (const auto& c : kFontSizeChoices) {
        SendMessageW(app.cmb_font_size, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(c.label));
    }
    app.tab0_ctrls.push_back(app.cmb_font_size);

    add_label(app.tab0_ctrls, L"變更候選字窗的排列方向與外觀主題。", 38, 164, 400, 18);
    add_label(app.tab0_ctrls, L"候選窗方向(O):", 56, 188, 110, 18);
    app.cmb_orientation = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
        app.S(172), app.S(184), app.S(160), app.S(120),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCmbOrientation)), app.hInst, nullptr);
    ApplyFont(app.cmb_orientation, app.font);
    SendMessageW(app.cmb_orientation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"直排"));
    SendMessageW(app.cmb_orientation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"橫排"));
    app.tab0_ctrls.push_back(app.cmb_orientation);

    add_label(app.tab0_ctrls, L"外觀主題(M):", 56, 218, 110, 18);
    app.cmb_theme = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
        app.S(172), app.S(214), app.S(160), app.S(120),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCmbTheme)), app.hInst, nullptr);
    ApplyFont(app.cmb_theme, app.font);
    SendMessageW(app.cmb_theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"淺色主題 (Light)"));
    SendMessageW(app.cmb_theme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"深色主題 (Dark)"));
    app.tab0_ctrls.push_back(app.cmb_theme);

    app.chk_associate = CreateWindowExW(
        0, L"BUTTON", L"提示相關字詞(P)",
        WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX,
        app.S(42), app.S(248), app.S(180), app.S(22),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdChkAssociate)), app.hInst, nullptr);
    ApplyFont(app.chk_associate, app.font);
    app.tab0_ctrls.push_back(app.chk_associate);

    add_label(app.tab0_ctrls, L"輸出設定:", 26, 280, 70, 18);
    add_sep(app.tab0_ctrls, 96, 288, 360);

    add_label(app.tab0_ctrls, L"選擇輸出字種。", 38, 302, 200, 18);
    app.rad_traditional = CreateWindowExW(
        0, L"BUTTON", L"繁體字 (T)",
        WS_CHILD | WS_GROUP | WS_TABSTOP | BS_AUTORADIOBUTTON,
        app.S(64), app.S(324), app.S(140), app.S(20),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRadTraditional)), app.hInst, nullptr);
    app.rad_simplified = CreateWindowExW(
        0, L"BUTTON", L"简体字 (S)",
        WS_CHILD | WS_TABSTOP | BS_AUTORADIOBUTTON,
        app.S(64), app.S(346), app.S(140), app.S(20),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRadSimplified)), app.hInst, nullptr);
    ApplyFont(app.rad_traditional, app.font);
    ApplyFont(app.rad_simplified, app.font);
    app.tab0_ctrls.push_back(app.rad_traditional);
    app.tab0_ctrls.push_back(app.rad_simplified);

    add_label(app.tab0_ctrls, L"選擇輸出字元集 (請確定已經安裝所需字型檔案)。", 38, 374, 400, 18);
    app.btn_charset = CreateWindowExW(
        0, L"BUTTON", L"字元集設定(C)...",
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        app.S(56), app.S(398), app.S(130), app.S(26),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBtnCharset)), app.hInst, nullptr);
    ApplyFont(app.btn_charset, app.font);
    app.tab0_ctrls.push_back(app.btn_charset);

    // Tab 1: 進階
    add_label(app.tab1_ctrls, L"設定(S):", 26, 48, 60, 18);
    add_sep(app.tab1_ctrls, 88, 56, 368);

    add_label(app.tab1_ctrls, L"您可以個人化有關輸入法細節的選項", 42, 74, 240, 20);
    app.btn_reset = CreateWindowExW(
        0, L"BUTTON", L"還原全部的設定成預設值(R)...",
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        app.S(286), app.S(68), app.S(172), app.S(26),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBtnResetDefaults)), app.hInst, nullptr);
    ApplyFont(app.btn_reset, app.font);
    app.tab1_ctrls.push_back(app.btn_reset);

    app.list_adv = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        app.S(42), app.S(102), app.S(416), app.S(142),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdListAdvanced)), app.hInst, nullptr);
    ApplyFont(app.list_adv, app.font);
    ListView_SetExtendedListViewStyle(app.list_adv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = app.S(180);
    col.pszText = const_cast<LPWSTR>(L"名稱");
    ListView_InsertColumn(app.list_adv, 0, &col);
    col.cx = app.S(230);
    col.pszText = const_cast<LPWSTR>(L"值");
    ListView_InsertColumn(app.list_adv, 1, &col);

    for (int i = 0; i < 4; ++i) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = i;
        item.pszText = const_cast<LPWSTR>(kAdvItems[i].name);
        ListView_InsertItem(app.list_adv, &item);
    }
    ListView_SetItemState(app.list_adv, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    app.tab1_ctrls.push_back(app.list_adv);

    app.lbl_adv_desc = add_label(app.tab1_ctrls, L"", 42, 256, 416, 44, kIdLblAdvDesc);

    app.rad_opt0 = CreateWindowExW(
        0, L"BUTTON", L"",
        WS_CHILD | WS_GROUP | WS_TABSTOP | BS_AUTORADIOBUTTON,
        app.S(52), app.S(314), app.S(240), app.S(22),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRadOpt0)), app.hInst, nullptr);
    app.rad_opt1 = CreateWindowExW(
        0, L"BUTTON", L"",
        WS_CHILD | WS_TABSTOP | BS_AUTORADIOBUTTON,
        app.S(52), app.S(340), app.S(240), app.S(22),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdRadOpt1)), app.hInst, nullptr);
    ApplyFont(app.rad_opt0, app.font);
    ApplyFont(app.rad_opt1, app.font);
    app.tab1_ctrls.push_back(app.rad_opt0);
    app.tab1_ctrls.push_back(app.rad_opt1);

    // Tab 2: 鍵盤
    add_label(app.tab2_ctrls, L"鍵盤對應:", 26, 48, 70, 18);
    add_sep(app.tab2_ctrls, 96, 56, 360);

    add_label(app.tab2_ctrls, L"選擇輸入時的鍵盤對應方式(P)。", 38, 78, 360, 18);
    app.cmb_layout = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST,
        app.S(112), app.S(114), app.S(180), app.S(140),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCmbLayout)), app.hInst, nullptr);
    ApplyFont(app.cmb_layout, app.font);
    SendMessageW(app.cmb_layout, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"標準注音鍵盤"));
    SendMessageW(app.cmb_layout, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"倚天注音鍵盤"));
    SendMessageW(app.cmb_layout, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"IBM 注音鍵盤"));
    SendMessageW(app.cmb_layout, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"精業注音鍵盤"));
    app.tab2_ctrls.push_back(app.cmb_layout);

    app.kbd_preview = CreateWindowExW(
        0, kKbdPreviewClass, L"",
        WS_CHILD,
        app.S(26), app.S(168), app.S(436), app.S(172),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdKbdPreview)), app.hInst, nullptr);
    SetWindowLongPtrW(app.kbd_preview, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));
    app.tab2_ctrls.push_back(app.kbd_preview);

    // Tab 3: 設定檔
    add_label(app.tab3_ctrls, L"設定檔:", 26, 48, 60, 18);
    add_sep(app.tab3_ctrls, 88, 56, 368);

    add_label(app.tab3_ctrls, L"您可以將目前的 Z注音 設定匯出為 YAML 檔案備份，或從 YAML 檔案匯入設定。", 38, 76, 412, 36);

    add_label(app.tab3_ctrls, L"將目前的輸入法設定匯出為 .yaml 檔案：", 42, 128, 360, 18);
    app.btn_export = CreateWindowExW(
        0, L"BUTTON", L"匯出(E)...",
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        app.S(56), app.S(154), app.S(120), app.S(28),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBtnExportProfile)), app.hInst, nullptr);
    ApplyFont(app.btn_export, app.font);
    app.tab3_ctrls.push_back(app.btn_export);

    add_label(app.tab3_ctrls, L"從 .yaml 檔案載入設定（匯入後請按「確認」套用並儲存）：", 42, 204, 400, 18);
    app.btn_import = CreateWindowExW(
        0, L"BUTTON", L"匯入(I)...",
        WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
        app.S(56), app.S(230), app.S(120), app.S(28),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBtnImportProfile)), app.hInst, nullptr);
    ApplyFont(app.btn_import, app.font);
    app.tab3_ctrls.push_back(app.btn_import);

    LoadConfigIntoUi(app);
    ShowTabPage(app, 0);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<SettingsApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<SettingsApp*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->hwnd = hwnd;
        app->dpi = QueryDpi(hwnd);
        app->font = CreateUiFont(app->dpi, 9);
        app->kbd_font = CreateUiFont(app->dpi, 9);
        RebuildThemeBrushes(*app);
    }

    if (!app) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_CREATE:
            BuildControls(*app);
            return 0;

        case WM_ERASEBKGND:
            if (app->win_brush) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wParam), &rc, app->win_brush);
                return 1;
            }
            break;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HWND ctrl = reinterpret_cast<HWND>(lParam);
            HDC hdc = reinterpret_cast<HDC>(wParam);
            const ThemeColors tc = GetThemeColors(app->cfg.ui.dark_theme);
            SetTextColor(hdc, tc.text);
            if (ctrl == app->btn_ok || ctrl == app->btn_cancel) {
                SetBkColor(hdc, tc.win_bg);
                return reinterpret_cast<LRESULT>(app->win_brush);
            }
            SetBkColor(hdc, tc.page_bg);
            return reinterpret_cast<LRESULT>(app->page_brush);
        }

        case WM_NOTIFY: {
            auto* nm = reinterpret_cast<NMHDR*>(lParam);
            if (nm->idFrom == kIdTab && nm->code == TCN_SELCHANGE) {
                ShowTabPage(*app, TabCtrl_GetCurSel(app->tab));
                InvalidateRect(app->tab, nullptr, TRUE);
                return 0;
            }
            if (nm->idFrom == kIdListAdvanced && nm->code == LVN_ITEMCHANGED) {
                auto* lv = reinterpret_cast<NMLISTVIEW*>(lParam);
                if ((lv->uNewState & LVIS_SELECTED) && lv->iItem >= 0 && lv->iItem < 4) {
                    app->selected_adv_row = lv->iItem;
                    SyncAdvancedDetail(*app);
                }
                return 0;
            }
            break;
        }

        case WM_COMMAND: {
            const int id = LOWORD(wParam);
            const int code = HIWORD(wParam);

            if (id == kIdCmbDefaultMode && code == CBN_SELCHANGE) {
                app->cfg.engine.default_native_mode =
                    (SendMessageW(app->cmb_default_mode, CB_GETCURSEL, 0, 0) == 0);
                return 0;
            }
            if (id == kIdCmbFontSize && code == CBN_SELCHANGE) {
                const LRESULT sel = SendMessageW(app->cmb_font_size, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < 4) app->cfg.ui.font_size_pt = kFontSizeChoices[sel].pt;
                return 0;
            }
            if (id == kIdCmbOrientation && code == CBN_SELCHANGE) {
                app->cfg.engine.orientation =
                    (SendMessageW(app->cmb_orientation, CB_GETCURSEL, 0, 0) == 0)
                    ? zzhuyin::CandidateOrientation::Vertical
                    : zzhuyin::CandidateOrientation::Horizontal;
                return 0;
            }
            if (id == kIdCmbTheme && code == CBN_SELCHANGE) {
                app->cfg.ui.dark_theme =
                    (SendMessageW(app->cmb_theme, CB_GETCURSEL, 0, 0) == 1);
                ApplyThemeToMainWindow(*app);
                return 0;
            }
            if (id == kIdChkAssociate && code == BN_CLICKED) {
                app->cfg.engine.prompt_associate_phrase =
                    (SendMessageW(app->chk_associate, BM_GETCHECK, 0, 0) == BST_CHECKED);
                return 0;
            }
            if (id == kIdRadTraditional && code == BN_CLICKED) {
                app->cfg.engine.output_simplified = false;
                return 0;
            }
            if (id == kIdRadSimplified && code == BN_CLICKED) {
                app->cfg.engine.output_simplified = true;
                return 0;
            }
            if (id == kIdBtnCharset && code == BN_CLICKED) {
                ShowCharsetDialog(*app);
                return 0;
            }
            if (id == kIdRadOpt0 && code == BN_CLICKED) {
                SetAdvValue(app->cfg, app->selected_adv_row, true);
                RefreshAdvancedList(*app);
                return 0;
            }
            if (id == kIdRadOpt1 && code == BN_CLICKED) {
                SetAdvValue(app->cfg, app->selected_adv_row, false);
                RefreshAdvancedList(*app);
                return 0;
            }
            if (id == kIdBtnResetDefaults && code == BN_CLICKED) {
                if (MessageBoxW(hwnd, L"確定要還原全部設定為預設值嗎？", L"Z注音 設定",
                                MB_YESNO | MB_ICONQUESTION) == IDYES) {
                    app->cfg = PersistedConfig{};
                    LoadConfigIntoUi(*app);
                }
                return 0;
            }
            if (id == kIdCmbLayout && code == CBN_SELCHANGE) {
                const LRESULT sel = SendMessageW(app->cmb_layout, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < 4) {
                    app->cfg.engine.layout = static_cast<zzhuyin::KeyboardLayout>(sel + 1);
                    InvalidateRect(app->kbd_preview, nullptr, TRUE);
                }
                return 0;
            }
            if (id == kIdBtnExportProfile && code == BN_CLICKED) {
                wchar_t file_buf[MAX_PATH] = L"zzhuyin-settings.yaml";
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = L"YAML 設定檔 (*.yaml;*.yml)\0*.yaml;*.yml\0所有檔案 (*.*)\0*.*\0";
                ofn.lpstrFile = file_buf;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrDefExt = L"yaml";
                ofn.lpstrTitle = L"匯出 Z注音 設定檔";
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetSaveFileNameW(&ofn)) {
                    if (ConfigStore::ExportYamlFile(app->cfg, file_buf)) {
                        MessageBoxW(hwnd, L"設定檔已成功匯出。", L"Z注音 設定", MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hwnd, L"無法寫入設定檔，請確認檔案路徑與權限。", L"Z注音 設定", MB_OK | MB_ICONERROR);
                    }
                }
                return 0;
            }
            if (id == kIdBtnImportProfile && code == BN_CLICKED) {
                wchar_t file_buf[MAX_PATH] = {};
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = L"YAML 設定檔 (*.yaml;*.yml)\0*.yaml;*.yml\0所有檔案 (*.*)\0*.*\0";
                ofn.lpstrFile = file_buf;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrTitle = L"匯入 Z注音 設定檔";
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameW(&ofn)) {
                    PersistedConfig imported{};
                    if (ConfigStore::ImportYamlFile(file_buf, imported)) {
                        app->cfg = imported;
                        LoadConfigIntoUi(*app);
                        MessageBoxW(hwnd, L"設定檔已匯入，請按「確認」以套用並儲存設定。", L"Z注音 設定", MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hwnd, L"無法讀取或解析設定檔，請確認為有效的 YAML 設定檔。", L"Z注音 設定", MB_OK | MB_ICONERROR);
                    }
                }
                return 0;
            }
            if (id == kIdBtnOk && code == BN_CLICKED) {
                ConfigStore::Save(app->cfg);
                if (app->on_saved) {
                    app->on_saved(app->cfg);
                }
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == kIdBtnCancel && code == BN_CLICKED) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }

        case WM_DESTROY:
            if (app->tab) RemoveWindowSubclass(app->tab, TabSubclassProc, 1);
            if (app->font) { DeleteObject(app->font); app->font = nullptr; }
            if (app->kbd_font) { DeleteObject(app->kbd_font); app->kbd_font = nullptr; }
            if (app->win_brush) { DeleteObject(app->win_brush); app->win_brush = nullptr; }
            if (app->page_brush) { DeleteObject(app->page_brush); app->page_brush = nullptr; }
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RunSettingsThread(HINSTANCE hInst, std::function<void(const PersistedConfig&)> on_saved) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;

    wc.lpfnWndProc = MainWndProc;
    wc.lpszClassName = kMainWndClass;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = CharsetDlgProc;
    wc.lpszClassName = kCharsetWndClass;
    RegisterClassExW(&wc);

    wc.lpfnWndProc = KeyboardPreviewProc;
    wc.lpszClassName = kKbdPreviewClass;
    RegisterClassExW(&wc);

    SettingsApp app{};
    app.hInst = hInst;
    app.cfg = ConfigStore::Load();
    app.on_saved = std::move(on_saved);

    HDC hdc = GetDC(nullptr);
    app.dpi = hdc ? static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSX)) : 96;
    if (hdc) ReleaseDC(nullptr, hdc);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc{0, 0, app.S(488), app.S(494)};
    AdjustWindowRectEx(&rc, style, FALSE, 0);
    const int win_w = rc.right - rc.left;
    const int win_h = rc.bottom - rc.top;
    const int pos_x = (GetSystemMetrics(SM_CXSCREEN) - win_w) / 2;
    const int pos_y = (GetSystemMetrics(SM_CYSCREEN) - win_h) / 2;

    HWND hwnd = CreateWindowExW(
        0, kMainWndClass, L"Z注音 設定",
        style,
        pos_x, pos_y, win_w, win_h,
        nullptr, nullptr, hInst, &app);

    if (hwnd) {
        HICON app_icon = LangBarButton::RenderModeIcon(/*is_native=*/true, /*is_full_shape=*/false);
        if (app_icon) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(app_icon));
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(app_icon));
        }

        g_activeSettingsWnd.store(hwnd);
        ShowWindow(hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd);
        UpdateWindow(hwnd);

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (!IsDialogMessageW(hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        g_activeSettingsWnd.store(nullptr);
        if (app_icon) {
            DestroyIcon(app_icon);
        }
    }

    DllRelease();
}

} // namespace

void ShowSettingsDialog(
    HINSTANCE hInst,
    std::function<void(const PersistedConfig&)> on_saved) {
    HWND existing = g_activeSettingsWnd.load();
    if (existing && IsWindow(existing)) {
        ShowWindow(existing, SW_RESTORE);
        SetForegroundWindow(existing);
        return;
    }

    DllAddRef();
    std::thread(RunSettingsThread, hInst, std::move(on_saved)).detach();
}

} // namespace zzhuyin::tsf
