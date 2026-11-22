#pragma once

#include <windows.h>
#include <functional>
#include <vector>

namespace zzhuyin::tsf {

class SymbolKeyboardWindow {
public:
    SymbolKeyboardWindow() = default;
    ~SymbolKeyboardWindow();

    SymbolKeyboardWindow(const SymbolKeyboardWindow&) = delete;
    SymbolKeyboardWindow& operator=(const SymbolKeyboardWindow&) = delete;

    bool Create(HINSTANCE hInst);
    void Destroy();

    void SetDarkTheme(bool dark);
    void Show();
    void Hide();
    bool IsVisible() const;

    void SetSymbolCallback(std::function<void(char16_t)> cb) {
        on_symbol_ = std::move(cb);
    }
    void SetCloseCallback(std::function<void()> cb) {
        on_close_ = std::move(cb);
    }

private:
    struct KeyCell {
        RECT rc{};
        const wchar_t* top_label = nullptr;   // QWERTY label (top-left)
        const wchar_t* fixed_label = nullptr; // Center label for Tab/Caps/Shift/Ctrl/Esc/Alt/Enter
        char16_t symbol = 0;                  // Full-width symbol (bottom-right)
        bool is_esc = false;
    };

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void RebuildFonts();
    void ComputeLayout();
    void Paint(HDC hdc);
    int Scale(int px) const;

    HWND hwnd_ = nullptr;
    HFONT label_font_ = nullptr;
    HFONT symbol_font_ = nullptr;
    HFONT fixed_font_ = nullptr;
    UINT dpi_ = 96;
    bool dark_theme_ = false;
    bool positioned_ = false;

    RECT close_rc_{};
    RECT drag_bar_rc_{};
    std::vector<KeyCell> cells_;
    int hover_cell_ = -1;
    bool hover_close_ = false;
    bool tracking_mouse_ = false;

    bool dragging_ = false;
    POINT drag_start_cursor_{};
    POINT drag_start_wnd_{};

    std::function<void(char16_t)> on_symbol_;
    std::function<void()> on_close_;
};

} // namespace zzhuyin::tsf
