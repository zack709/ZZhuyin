#include "tsf/candidate_wnd.h"

#include <algorithm>
#include <string>

namespace zzhuyin::tsf {

namespace {

constexpr wchar_t kCandidateWndClass[] = L"ZZhuyinCandidateWnd";
constexpr int kPadXBase      = 10;
constexpr int kPadYBase      = 6;
constexpr int kItemGapBase   = 4;
constexpr int kMinWidthBase  = 140;

std::wstring ToWide(const std::u16string& s) {
    return std::wstring(reinterpret_cast<const wchar_t*>(s.data()), s.size());
}

UINT QueryWindowDpi(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto fn = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (fn && hwnd) {
            const UINT dpi = fn(hwnd);
            if (dpi > 0) return dpi;
        }
    }
    return 96;
}

} // namespace

CandidateWindow::~CandidateWindow() {
    Destroy();
}

int CandidateWindow::Scale(int px) const {
    return MulDiv(px, static_cast<int>(dpi_), 96);
}

void CandidateWindow::SetOrientation(zzhuyin::CandidateOrientation orientation) {
    orientation_ = orientation;
}

void CandidateWindow::SetFontSizePt(int font_size_pt) {
    const int clamped = std::clamp(font_size_pt, 9, 28);
    if (clamped != font_size_pt_) {
        font_size_pt_ = clamped;
        if (hwnd_) {
            RebuildFont();
        }
    }
}

void CandidateWindow::SetDarkTheme(bool dark) {
    if (dark_theme_ != dark) {
        dark_theme_ = dark;
        if (hwnd_ && view_.active) {
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }
}

void CandidateWindow::RebuildFont() {
    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(font_size_pt_, static_cast<int>(dpi_), 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcsncpy_s(lf.lfFaceName, L"Microsoft JhengHei UI", _TRUNCATE);
    font_ = CreateFontIndirectW(&lf);
}

bool CandidateWindow::Create(HINSTANCE hInst) {
    if (hwnd_) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kCandidateWndClass;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kCandidateWndClass,
        L"",
        WS_POPUP | WS_BORDER,
        0, 0, 100, 100,
        nullptr, nullptr, hInst, this);

    if (!hwnd_) {
        return false;
    }

    dpi_ = QueryWindowDpi(hwnd_);
    RebuildFont();
    return true;
}

void CandidateWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
}

void CandidateWindow::Hide() {
    view_.active = false;
    hover_index_ = -1;
    tracking_mouse_ = false;
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

std::wstring CandidateWindow::FormatItemText(size_t index) const {
    if (index >= view_.page_items.size()) return {};
    return view_.is_associate
        ? (L"S+" + std::to_wstring(index + 1) + L". " + ToWide(view_.page_items[index]))
        : (std::to_wstring(index + 1) + L". " + ToWide(view_.page_items[index]));
}

std::wstring CandidateWindow::FormatFooterText() const {
    const size_t cur_page   = (view_.page_start / 9) + 1;
    const size_t total_page = (view_.total_count + 8) / 9;
    return view_.is_associate
        ? (L"相關 " + std::to_wstring(cur_page) + L"/" + std::to_wstring(total_page))
        : (std::to_wstring(cur_page) + L"/" + std::to_wstring(total_page));
}

SIZE CandidateWindow::ComputeLayout() {
    item_rects_.clear();
    footer_rect_ = RECT{};

    const int pad_x = Scale(kPadXBase);
    const int pad_y = Scale(kPadYBase);
    const int gap   = Scale(kItemGapBase);

    int line_h = Scale(24);
    std::vector<int> item_widths(view_.page_items.size(), Scale(48));
    int footer_w = Scale(48);

    if (hwnd_ && font_) {
        HDC hdc = GetDC(hwnd_);
        if (hdc) {
            HGDIOBJ old_font = SelectObject(hdc, font_);
            TEXTMETRICW tm{};
            if (GetTextMetricsW(hdc, &tm)) {
                line_h = tm.tmHeight + Scale(8);
            }
            for (size_t i = 0; i < view_.page_items.size(); ++i) {
                const std::wstring text = FormatItemText(i);
                SIZE sz{};
                if (GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &sz)) {
                    item_widths[i] = static_cast<int>(sz.cx) + Scale(14);
                }
            }
            const std::wstring footer = FormatFooterText();
            SIZE fsz{};
            if (GetTextExtentPoint32W(hdc, footer.c_str(), static_cast<int>(footer.size()), &fsz)) {
                footer_w = static_cast<int>(fsz.cx) + Scale(12);
            }
            SelectObject(hdc, old_font);
            ReleaseDC(hwnd_, hdc);
        }
    }

    if (orientation_ == zzhuyin::CandidateOrientation::Horizontal) {
        int x = pad_x;
        const int y = pad_y;
        item_rects_.reserve(view_.page_items.size());
        for (size_t i = 0; i < view_.page_items.size(); ++i) {
            item_rects_.push_back(RECT{x, y, x + item_widths[i], y + line_h});
            x += item_widths[i] + gap;
        }
        footer_rect_ = RECT{x, y, x + footer_w, y + line_h};
        const int total_w = x + footer_w + pad_x;
        const int total_h = pad_y * 2 + line_h;
        return SIZE{total_w, total_h};
    }

    // Vertical orientation
    int max_w = std::max(Scale(kMinWidthBase), footer_w + pad_x * 2);
    for (int w : item_widths) {
        max_w = std::max(max_w, w + pad_x * 2);
    }

    int y = pad_y;
    item_rects_.reserve(view_.page_items.size());
    for (size_t i = 0; i < view_.page_items.size(); ++i) {
        item_rects_.push_back(RECT{Scale(4), y, max_w - Scale(4), y + line_h});
        y += line_h;
    }
    const int footer_h = std::max(Scale(18), line_h - Scale(4));
    footer_rect_ = RECT{pad_x, y + Scale(2), max_w - pad_x, y + footer_h};
    return SIZE{max_w, y + footer_h + pad_y};
}

void CandidateWindow::PositionNearCaret(const RECT& caret_rect, SIZE win_size) {
    int x = caret_rect.left;
    int y = caret_rect.bottom + Scale(4);

    HMONITOR mon = MonitorFromRect(&caret_rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi)) {
        const RECT& wa = mi.rcWork;
        if (y + win_size.cy > wa.bottom) {
            y = caret_rect.top - win_size.cy - Scale(4);
        }
        if (x + win_size.cx > wa.right) {
            x = wa.right - win_size.cx;
        }
        if (x < wa.left) x = wa.left;
        if (y < wa.top)  y = wa.top;
    }

    SetWindowPos(
        hwnd_, HWND_TOPMOST,
        x, y, win_size.cx, win_size.cy,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void CandidateWindow::Update(const CandidateView& view, const RECT& caret_rect) {
    view_ = view;
    hover_index_ = -1;
    if (!hwnd_ || !view_.active || view_.page_items.empty()) {
        Hide();
        return;
    }

    const UINT new_dpi = QueryWindowDpi(hwnd_);
    if (new_dpi != dpi_) {
        dpi_ = new_dpi;
        RebuildFont();
    }

    const SIZE sz = ComputeLayout();
    PositionNearCaret(caret_rect, sz);
    InvalidateRect(hwnd_, nullptr, FALSE);
    UpdateWindow(hwnd_);
}

void CandidateWindow::Paint(HDC hdc) {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    // Double-buffered painting to eliminate flicker
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ old_bmp = SelectObject(mem_dc, mem_bmp);

    const COLORREF bg_col       = dark_theme_ ? RGB(32, 32, 32)   : RGB(252, 252, 252);
    const COLORREF sel_bg_col   = dark_theme_ ? RGB(15, 118, 110) : RGB(0, 120, 215);
    const COLORREF hov_bg_col   = dark_theme_ ? RGB(52, 52, 52)   : RGB(229, 241, 251);
    const COLORREF text_col     = dark_theme_ ? RGB(240, 240, 240): RGB(32, 32, 32);
    const COLORREF hov_text_col = dark_theme_ ? RGB(255, 255, 255): RGB(24, 24, 24);
    const COLORREF footer_col   = dark_theme_ ? RGB(165, 165, 165): RGB(110, 110, 110);

    HBRUSH bg_brush = CreateSolidBrush(bg_col);
    FillRect(mem_dc, &rc, bg_brush);
    DeleteObject(bg_brush);

    HGDIOBJ old_font = font_ ? SelectObject(mem_dc, font_) : nullptr;
    SetBkMode(mem_dc, TRANSPARENT);

    const size_t local_sel = (view_.selected_index >= view_.page_start)
                           ? (view_.selected_index - view_.page_start)
                           : 0;

    for (size_t i = 0; i < view_.page_items.size() && i < item_rects_.size(); ++i) {
        const RECT& item_rc = item_rects_[i];
        const bool selected = (i == local_sel);
        const bool hovered  = (!selected && static_cast<int>(i) == hover_index_);

        if (selected) {
            HBRUSH sel_brush = CreateSolidBrush(sel_bg_col);
            FillRect(mem_dc, &item_rc, sel_brush);
            DeleteObject(sel_brush);
            SetTextColor(mem_dc, RGB(255, 255, 255));
        } else if (hovered) {
            HBRUSH hov_brush = CreateSolidBrush(hov_bg_col);
            FillRect(mem_dc, &item_rc, hov_brush);
            DeleteObject(hov_brush);
            SetTextColor(mem_dc, hov_text_col);
        } else {
            SetTextColor(mem_dc, text_col);
        }

        const std::wstring line = FormatItemText(i);
        RECT text_rc = item_rc;
        text_rc.left += Scale(6);
        DrawTextW(mem_dc, line.c_str(), static_cast<int>(line.size()), &text_rc,
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
    }

    const std::wstring footer = FormatFooterText();
    SetTextColor(mem_dc, footer_col);
    DrawTextW(mem_dc, footer.c_str(), static_cast<int>(footer.size()), &footer_rect_,
              DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);

    if (old_font) {
        SelectObject(mem_dc, old_font);
    }

    BitBlt(hdc, 0, 0, width, height, mem_dc, 0, 0, SRCCOPY);
    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);
}

LRESULT CALLBACK CandidateWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    CandidateWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<CandidateWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CandidateWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_ERASEBKGND:
            return 1; // Handled in double-buffered Paint

        case WM_MOUSEMOVE: {
            if (!tracking_mouse_) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd_;
                TrackMouseEvent(&tme);
                tracking_mouse_ = true;
            }
            const POINT pt{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
            int new_hover = -1;
            for (size_t i = 0; i < item_rects_.size(); ++i) {
                if (PtInRect(&item_rects_[i], pt)) {
                    new_hover = static_cast<int>(i);
                    break;
                }
            }
            if (new_hover != hover_index_) {
                hover_index_ = new_hover;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            if (hover_index_ != -1) {
                hover_index_ = -1;
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            const POINT pt{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
            for (size_t i = 0; i < item_rects_.size(); ++i) {
                if (PtInRect(&item_rects_[i], pt)) {
                    if (on_select_) {
                        on_select_(i);
                    }
                    break;
                }
            }
            return 0;
        }

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
