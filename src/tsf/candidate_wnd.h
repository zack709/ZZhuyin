#pragma once

#include "core/types.h"

#include <windows.h>
#include <functional>
#include <vector>

namespace zzhuyin::tsf {

class CandidateWindow {
public:
    CandidateWindow() = default;
    ~CandidateWindow();

    CandidateWindow(const CandidateWindow&) = delete;
    CandidateWindow& operator=(const CandidateWindow&) = delete;

    bool Create(HINSTANCE hInst);
    void Destroy();

    void SetOrientation(zzhuyin::CandidateOrientation orientation);
    void SetFontSizePt(int font_size_pt);
    void SetDarkTheme(bool dark);

    void Update(const CandidateView& view, const RECT& caret_rect);
    void Hide();

    void SetSelectCallback(std::function<void(size_t page_offset)> cb) {
        on_select_ = std::move(cb);
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void Paint(HDC hdc);
    void RebuildFont();
    SIZE ComputeLayout();
    void PositionNearCaret(const RECT& caret_rect, SIZE win_size);
    int Scale(int px) const;
    std::wstring FormatItemText(size_t index) const;
    std::wstring FormatFooterText() const;

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    UINT dpi_ = 96;
    int font_size_pt_ = 12;
    bool dark_theme_ = false;
    zzhuyin::CandidateOrientation orientation_ = zzhuyin::CandidateOrientation::Vertical;

    CandidateView view_{};
    std::vector<RECT> item_rects_;
    RECT footer_rect_{};
    int hover_index_ = -1;
    bool tracking_mouse_ = false;

    std::function<void(size_t)> on_select_;
};

} // namespace zzhuyin::tsf
