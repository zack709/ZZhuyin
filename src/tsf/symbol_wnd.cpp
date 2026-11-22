#include "tsf/symbol_wnd.h"

#include "core/key_mapper.h"

#include <algorithm>
#include <string>

namespace zzhuyin::tsf {

namespace {

constexpr wchar_t kSymbolWndClass[] = L"ZZhuyinSymbolKeyboardWnd";
constexpr int kBaseWidth   = 476;
constexpr int kBaseHeight  = 186;
constexpr int kTitleBarH   = 22;

UINT QueryWindowDpi(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using Fn = UINT(WINAPI*)(HWND);
        auto fn = reinterpret_cast<Fn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (fn && hwnd) {
            const UINT d = fn(hwnd);
            if (d > 0) return d;
        }
    }
    return 96;
}

} // namespace

SymbolKeyboardWindow::~SymbolKeyboardWindow() {
    Destroy();
}

int SymbolKeyboardWindow::Scale(int px) const {
    return MulDiv(px, static_cast<int>(dpi_), 96);
}

void SymbolKeyboardWindow::RebuildFonts() {
    if (label_font_)  { DeleteObject(label_font_);  label_font_  = nullptr; }
    if (symbol_font_) { DeleteObject(symbol_font_); symbol_font_ = nullptr; }
    if (fixed_font_)  { DeleteObject(fixed_font_);  fixed_font_  = nullptr; }

    auto make_font = [&](int pt, int weight) -> HFONT {
        LOGFONTW lf{};
        lf.lfHeight = -MulDiv(pt, static_cast<int>(dpi_), 72);
        lf.lfWeight = weight;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        wcsncpy_s(lf.lfFaceName, L"Microsoft JhengHei UI", _TRUNCATE);
        return CreateFontIndirectW(&lf);
    };

    label_font_  = make_font(8, FW_SEMIBOLD);
    symbol_font_ = make_font(11, FW_NORMAL);
    fixed_font_  = make_font(8, FW_NORMAL);
}

bool SymbolKeyboardWindow::Create(HINSTANCE hInst) {
    if (hwnd_) return true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kSymbolWndClass;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kSymbolWndClass,
        L"",
        WS_POPUP | WS_BORDER,
        0, 0, kBaseWidth, kBaseHeight,
        nullptr, nullptr, hInst, this);

    if (!hwnd_) return false;

    dpi_ = QueryWindowDpi(hwnd_);
    RebuildFonts();
    ComputeLayout();
    return true;
}

void SymbolKeyboardWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (label_font_)  { DeleteObject(label_font_);  label_font_  = nullptr; }
    if (symbol_font_) { DeleteObject(symbol_font_); symbol_font_ = nullptr; }
    if (fixed_font_)  { DeleteObject(fixed_font_);  fixed_font_  = nullptr; }
}

void SymbolKeyboardWindow::SetDarkTheme(bool dark) {
    if (dark_theme_ != dark) {
        dark_theme_ = dark;
        if (hwnd_ && IsVisible()) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }
}

bool SymbolKeyboardWindow::IsVisible() const {
    return hwnd_ != nullptr && IsWindowVisible(hwnd_) != FALSE;
}

void SymbolKeyboardWindow::Show() {
    if (!hwnd_) return;

    const UINT new_dpi = QueryWindowDpi(hwnd_);
    if (new_dpi != dpi_) {
        dpi_ = new_dpi;
        RebuildFonts();
    }

    const int w = Scale(kBaseWidth);
    const int h = Scale(kBaseHeight);

    if (!positioned_) {
        POINT pt{};
        GetCursorPos(&pt);
        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        int x = 100;
        int y = 100;
        if (GetMonitorInfoW(mon, &mi)) {
            x = mi.rcWork.right - w - Scale(16);
            y = mi.rcWork.bottom - h - Scale(16);
        }
        SetWindowPos(hwnd_, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        positioned_ = true;
    } else {
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, w, h,
                     SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    ComputeLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
    UpdateWindow(hwnd_);
}

void SymbolKeyboardWindow::Hide() {
    hover_cell_ = -1;
    hover_close_ = false;
    dragging_ = false;
    tracking_mouse_ = false;
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void SymbolKeyboardWindow::ComputeLayout() {
    cells_.clear();
    if (!hwnd_) return;

    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int pad = Scale(4);
    const int gap = Scale(2);
    const int title_h = Scale(kTitleBarH);

    close_rc_ = RECT{w - pad - Scale(20), Scale(2), w - pad, title_h - Scale(2)};
    drag_bar_rc_ = RECT{0, 0, close_rc_.left - Scale(2), title_h};

    struct Spec {
        const wchar_t* top_label;
        const wchar_t* fixed_label;
        uint16_t vk;
        double units;
        bool is_esc;
    };

    const std::vector<std::vector<Spec>> rows = {
        {
            {L"`",  nullptr, zzhuyin::vk::kOem3,      1.0, false},
            {L"1",  nullptr, '1',                     1.0, false},
            {L"2",  nullptr, '2',                     1.0, false},
            {L"3",  nullptr, '3',                     1.0, false},
            {L"4",  nullptr, '4',                     1.0, false},
            {L"5",  nullptr, '5',                     1.0, false},
            {L"6",  nullptr, '6',                     1.0, false},
            {L"7",  nullptr, '7',                     1.0, false},
            {L"8",  nullptr, '8',                     1.0, false},
            {L"9",  nullptr, '9',                     1.0, false},
            {L"0",  nullptr, '0',                     1.0, false},
            {L"-",  nullptr, zzhuyin::vk::kOemMinus,  1.0, false},
            {L"=",  nullptr, zzhuyin::vk::kOemPlus,   1.0, false},
            {L"\\", nullptr, zzhuyin::vk::kOem5,      1.0, false},
            {nullptr, L"←",  0,                       1.1, false},
        },
        {
            {nullptr, L"Tab", 0,                      1.5, false},
            {L"Q",  nullptr, 'Q',                     1.0, false},
            {L"W",  nullptr, 'W',                     1.0, false},
            {L"E",  nullptr, 'E',                     1.0, false},
            {L"R",  nullptr, 'R',                     1.0, false},
            {L"T",  nullptr, 'T',                     1.0, false},
            {L"Y",  nullptr, 'Y',                     1.0, false},
            {L"U",  nullptr, 'U',                     1.0, false},
            {L"I",  nullptr, 'I',                     1.0, false},
            {L"O",  nullptr, 'O',                     1.0, false},
            {L"P",  nullptr, 'P',                     1.0, false},
            {L"[",  nullptr, zzhuyin::vk::kOem4,      1.0, false},
            {L"]",  nullptr, zzhuyin::vk::kOem6,      1.0, false},
            {nullptr, L"Enter", 0,                    1.6, false},
        },
        {
            {nullptr, L"Caps", 0,                     1.8, false},
            {L"A",  nullptr, 'A',                     1.0, false},
            {L"S",  nullptr, 'S',                     1.0, false},
            {L"D",  nullptr, 'D',                     1.0, false},
            {L"F",  nullptr, 'F',                     1.0, false},
            {L"G",  nullptr, 'G',                     1.0, false},
            {L"H",  nullptr, 'H',                     1.0, false},
            {L"J",  nullptr, 'J',                     1.0, false},
            {L"K",  nullptr, 'K',                     1.0, false},
            {L"L",  nullptr, 'L',                     1.0, false},
            {L";",  nullptr, zzhuyin::vk::kOem1,      1.0, false},
            {L"'",  nullptr, zzhuyin::vk::kOem7,      1.0, false},
            {nullptr, L"",   0,                       2.3, false}, // occupied by tall Enter
        },
        {
            {nullptr, L"Shift", 0,                    2.3, false},
            {L"Z",  nullptr, 'Z',                     1.0, false},
            {L"X",  nullptr, 'X',                     1.0, false},
            {L"C",  nullptr, 'C',                     1.0, false},
            {L"V",  nullptr, 'V',                     1.0, false},
            {L"B",  nullptr, 'B',                     1.0, false},
            {L"N",  nullptr, 'N',                     1.0, false},
            {L"M",  nullptr, 'M',                     1.0, false},
            {L",",  nullptr, zzhuyin::vk::kOemComma,  1.0, false},
            {L".",  nullptr, zzhuyin::vk::kOemPeriod, 1.0, false},
            {L"/",  nullptr, zzhuyin::vk::kOem2,      1.0, false},
            {nullptr, L"Shift", 0,                    2.8, false},
        },
        {
            {nullptr, L"Ctrl", 0,                     1.5, false},
            {nullptr, L"Esc",  0,                     1.5, true},
            {nullptr, L"Alt",  0,                     1.5, false},
            {nullptr, L"",     0,                     6.1, false},
            {nullptr, L"Alt",  0,                     1.5, false},
            {nullptr, L"",     0,                     1.5, false}, // gap
            {nullptr, L"Ctrl", 0,                     1.5, false},
        },
    };

    const int avail_w = w - pad * 2;
    const int avail_h = h - title_h - pad;
    const int row_h = (avail_h - gap * 4) / 5;

    for (size_t r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        double total_units = 0.0;
        for (const auto& s : row) total_units += s.units;
        const double unit_w =
            static_cast<double>(avail_w - gap * static_cast<int>(row.size() - 1)) / total_units;

        double x = static_cast<double>(pad);
        const int y = title_h + static_cast<int>(r) * (row_h + gap);

        for (size_t c = 0; c < row.size(); ++c) {
            const auto& s = row[c];
            const int x1 = static_cast<int>(x);
            const int x2 = (c + 1 == row.size())
                ? (w - pad)
                : static_cast<int>(x + s.units * unit_w);
            x += s.units * unit_w + gap;

            if (r == 2 && c == row.size() - 1) continue;
            if (r == 4 && c == 5) continue;

            KeyCell cell{};
            cell.rc = RECT{x1, y, x2, y + row_h};
            if (r == 1 && c == row.size() - 1) {
                cell.rc.bottom = y + row_h * 2 + gap;
            }
            cell.top_label = s.top_label;
            cell.fixed_label = s.fixed_label;
            cell.is_esc = s.is_esc;
            if (s.vk != 0) {
                if (auto sym = zzhuyin::KeyMapper::MapSymbolKeyboard(s.vk)) {
                    cell.symbol = *sym;
                }
            }
            cells_.push_back(cell);
        }
    }
}

void SymbolKeyboardWindow::Paint(HDC hdc) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ old_bmp = SelectObject(mem_dc, mem_bmp);

    const COLORREF bg_col       = dark_theme_ ? RGB(32, 32, 32)   : RGB(240, 240, 240);
    const COLORREF bar_line_col = dark_theme_ ? RGB(72, 72, 72)   : RGB(196, 196, 196);
    const COLORREF key_bg_col   = dark_theme_ ? RGB(50, 50, 50)   : RGB(255, 255, 255);
    const COLORREF key_hov_col  = dark_theme_ ? RGB(64, 88, 86)   : RGB(224, 242, 241);
    const COLORREF key_mut_col  = dark_theme_ ? RGB(42, 42, 42)   : RGB(244, 244, 244);
    const COLORREF key_brd_col  = dark_theme_ ? RGB(82, 82, 82)   : RGB(188, 188, 188);
    const COLORREF lbl_col      = dark_theme_ ? RGB(94, 234, 212) : RGB(15, 118, 110);
    const COLORREF sym_col      = dark_theme_ ? RGB(243, 244, 246): RGB(24, 24, 24);
    const COLORREF fixed_col    = dark_theme_ ? RGB(175, 175, 175): RGB(90, 90, 90);
    const COLORREF close_hov    = RGB(196, 43, 28);

    HBRUSH bg_br = CreateSolidBrush(bg_col);
    FillRect(mem_dc, &rc, bg_br);
    DeleteObject(bg_br);

    SetBkMode(mem_dc, TRANSPARENT);

    // Top bar: title + grip line + close button
    HGDIOBJ old_font = fixed_font_ ? SelectObject(mem_dc, fixed_font_) : nullptr;
    SetTextColor(mem_dc, fixed_col);
    RECT title_rc{Scale(8), Scale(2), Scale(100), Scale(kTitleBarH) - Scale(2)};
    DrawTextW(mem_dc, L"標點符號鍵盤", -1, &title_rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    HPEN line_pen = CreatePen(PS_SOLID, 1, bar_line_col);
    HGDIOBJ old_pen = SelectObject(mem_dc, line_pen);
    const int grip_y = Scale(kTitleBarH) / 2;
    MoveToEx(mem_dc, Scale(96), grip_y, nullptr);
    LineTo(mem_dc, close_rc_.left - Scale(8), grip_y);
    SelectObject(mem_dc, old_pen);
    DeleteObject(line_pen);

    // Close button (X)
    if (hover_close_) {
        HBRUSH cbr = CreateSolidBrush(close_hov);
        FillRect(mem_dc, &close_rc_, cbr);
        DeleteObject(cbr);
        SetTextColor(mem_dc, RGB(255, 255, 255));
    } else {
        SetTextColor(mem_dc, sym_col);
    }
    DrawTextW(mem_dc, L"✕", 1, const_cast<RECT*>(&close_rc_), DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    // Keys
    HPEN key_pen = CreatePen(PS_SOLID, 1, key_brd_col);
    old_pen = SelectObject(mem_dc, key_pen);

    for (size_t i = 0; i < cells_.size(); ++i) {
        const auto& cell = cells_[i];
        const bool clickable = (cell.symbol != 0 || cell.is_esc);
        const bool hovered = clickable && (static_cast<int>(i) == hover_cell_);

        const COLORREF fill = hovered ? key_hov_col : (clickable ? key_bg_col : key_mut_col);
        HBRUSH kbr = CreateSolidBrush(fill);
        HGDIOBJ prev_br = SelectObject(mem_dc, kbr);
        RoundRect(mem_dc, cell.rc.left, cell.rc.top, cell.rc.right, cell.rc.bottom, 4, 4);
        SelectObject(mem_dc, prev_br);
        DeleteObject(kbr);

        if (cell.top_label) {
            if (label_font_) SelectObject(mem_dc, label_font_);
            SetTextColor(mem_dc, lbl_col);
            RECT lrc{cell.rc.left + Scale(3), cell.rc.top + Scale(1), cell.rc.right - Scale(2), cell.rc.bottom};
            DrawTextW(mem_dc, cell.top_label, -1, &lrc, DT_SINGLELINE | DT_LEFT | DT_TOP | DT_NOPREFIX);
        }

        if (cell.symbol != 0) {
            if (symbol_font_) SelectObject(mem_dc, symbol_font_);
            SetTextColor(mem_dc, sym_col);
            wchar_t ch = static_cast<wchar_t>(cell.symbol);
            RECT src{cell.rc.left + Scale(2), cell.rc.top + Scale(6), cell.rc.right - Scale(3), cell.rc.bottom - Scale(1)};
            DrawTextW(mem_dc, &ch, 1, &src, DT_SINGLELINE | DT_RIGHT | DT_BOTTOM | DT_NOPREFIX);
        } else if (cell.fixed_label && cell.fixed_label[0] != L'\0') {
            if (fixed_font_) SelectObject(mem_dc, fixed_font_);
            SetTextColor(mem_dc, cell.is_esc ? sym_col : fixed_col);
            RECT frc = cell.rc;
            DrawTextW(mem_dc, cell.fixed_label, -1, &frc, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
        }
    }

    SelectObject(mem_dc, old_pen);
    DeleteObject(key_pen);
    if (old_font) SelectObject(mem_dc, old_font);

    BitBlt(hdc, 0, 0, w, h, mem_dc, 0, 0, SRCCOPY);
    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);
}

LRESULT CALLBACK SymbolKeyboardWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SymbolKeyboardWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<SymbolKeyboardWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<SymbolKeyboardWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT SymbolKeyboardWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE: {
            if (dragging_) {
                POINT cur{};
                GetCursorPos(&cur);
                const int nx = drag_start_wnd_.x + (cur.x - drag_start_cursor_.x);
                const int ny = drag_start_wnd_.y + (cur.y - drag_start_cursor_.y);
                SetWindowPos(hwnd_, HWND_TOPMOST, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
                return 0;
            }
            if (!tracking_mouse_) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd_;
                TrackMouseEvent(&tme);
                tracking_mouse_ = true;
            }
            const POINT pt{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
            const bool new_close = (PtInRect(&close_rc_, pt) != FALSE);
            int new_cell = -1;
            if (!new_close) {
                for (size_t i = 0; i < cells_.size(); ++i) {
                    if (PtInRect(&cells_[i].rc, pt)) {
                        new_cell = static_cast<int>(i);
                        break;
                    }
                }
            }
            if (new_close != hover_close_ || new_cell != hover_cell_) {
                hover_close_ = new_close;
                hover_cell_ = new_cell;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            if (hover_close_ || hover_cell_ != -1) {
                hover_close_ = false;
                hover_cell_ = -1;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            const POINT pt{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
            if (PtInRect(&close_rc_, pt)) {
                if (on_close_) on_close_();
                return 0;
            }
            if (PtInRect(&drag_bar_rc_, pt)) {
                dragging_ = true;
                GetCursorPos(&drag_start_cursor_);
                RECT wrc{};
                GetWindowRect(hwnd_, &wrc);
                drag_start_wnd_ = POINT{wrc.left, wrc.top};
                SetCapture(hwnd_);
                return 0;
            }
            for (const auto& cell : cells_) {
                if (PtInRect(&cell.rc, pt)) {
                    if (cell.is_esc) {
                        if (on_close_) on_close_();
                    } else if (cell.symbol != 0 && on_symbol_) {
                        on_symbol_(cell.symbol);
                    }
                    break;
                }
            }
            return 0;
        }

        case WM_LBUTTONUP:
            if (dragging_) {
                dragging_ = false;
                ReleaseCapture();
            }
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd_, &ps);
            Paint(hdc);
            EndPaint(hwnd_, &ps);
            return 0;
        }

        default:
            return DefWindowProcW(hwnd_, msg, wParam, lParam);
    }
}

} // namespace zzhuyin::tsf
