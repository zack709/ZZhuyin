#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace zzhuyin {

// Portable Windows Virtual-Key constants (no <windows.h> dependency in core)
namespace vk {
inline constexpr uint16_t kBackspace  = 0x08;
inline constexpr uint16_t kTab        = 0x09;
inline constexpr uint16_t kReturn     = 0x0D;
inline constexpr uint16_t kShift      = 0x10;
inline constexpr uint16_t kControl    = 0x11;
inline constexpr uint16_t kMenu       = 0x12; // Alt
inline constexpr uint16_t kCapital    = 0x14; // CapsLock
inline constexpr uint16_t kEscape     = 0x1B;
inline constexpr uint16_t kSpace      = 0x20;
inline constexpr uint16_t kPageUp     = 0x21;
inline constexpr uint16_t kPageDown   = 0x22;
inline constexpr uint16_t kLeft       = 0x25;
inline constexpr uint16_t kUp         = 0x26;
inline constexpr uint16_t kRight      = 0x27;
inline constexpr uint16_t kDown       = 0x28;
inline constexpr uint16_t kDelete     = 0x2E;

inline constexpr uint16_t kNumpad0    = 0x60;
inline constexpr uint16_t kNumpad9    = 0x69;
inline constexpr uint16_t kMultiply   = 0x6A;
inline constexpr uint16_t kAdd        = 0x6B;
inline constexpr uint16_t kSeparator  = 0x6C;
inline constexpr uint16_t kSubtract   = 0x6D;
inline constexpr uint16_t kDecimal    = 0x6E;
inline constexpr uint16_t kDivide     = 0x6F;

inline constexpr uint16_t kLShift     = 0xA0;
inline constexpr uint16_t kRShift     = 0xA1;
inline constexpr uint16_t kLControl   = 0xA2;
inline constexpr uint16_t kRControl   = 0xA3;

inline constexpr uint16_t kOem1       = 0xBA; // ;:
inline constexpr uint16_t kOemPlus    = 0xBB; // =+
inline constexpr uint16_t kOemComma   = 0xBC; // ,<
inline constexpr uint16_t kOemMinus   = 0xBD; // -_
inline constexpr uint16_t kOemPeriod  = 0xBE; // .>
inline constexpr uint16_t kOem2       = 0xBF; // /?
inline constexpr uint16_t kOem3       = 0xC0; // `~
inline constexpr uint16_t kOem4       = 0xDB; // [{
inline constexpr uint16_t kOem5       = 0xDC; // \|
inline constexpr uint16_t kOem6       = 0xDD; // ]}
inline constexpr uint16_t kOem7       = 0xDE; // '"
} // namespace vk

// Modifier bitmask matching phon.ime FUN_7ff2e293e48
namespace mod {
inline constexpr uint16_t kShift    = 0x0001;
inline constexpr uint16_t kControl  = 0x0002;
inline constexpr uint16_t kAlt      = 0x0004;
inline constexpr uint16_t kCapsLock = 0x0008;
inline constexpr uint16_t kKana     = 0x0010;
} // namespace mod

enum class KeyboardLayout : uint8_t {
    Standard = 1, // 大千
    Eten     = 2, // 倚天
    Ibm      = 3, // IBM
    GinYieh  = 4, // 精業
};

enum class EngineState : uint8_t {
    Idle = 0,
    Composing,
    CandidateHomophone,
    CandidateAssociate,
};

enum class CandidateOrientation : uint8_t {
    Horizontal = 0,
    Vertical   = 1,
};

enum class BopomofoSlot : uint8_t {
    Initial = 0, // 聲母: 1..21
    Medial  = 1, // 介音: 1..3
    Final   = 2, // 韻母: 1..13
    Tone    = 3, // 聲調: 1..5
};

struct CharsetFilterConfig {
    bool output_big5_only = false; // 0x0126 (Section 1 bit 0x02)
    bool include_ext_a    = false; // 0x0127 (Section 1 bit 0x04)
    bool include_ext_b    = false; // 0x0128 (IMTCPHCS.IMD)
    bool allow_cns        = false; // 0x0129 (Section 1 bit 0x01)
    bool include_hkscs    = false; // 0x012c (Section 1 bit 0x08)
};

struct EngineConfig {
    KeyboardLayout layout              = KeyboardLayout::Standard;
    CharsetFilterConfig charset_filter = {};
    bool prompt_associate_phrase       = false; // 0x0113
    bool default_native_mode           = true;  // 0x0114
    bool enable_punctuation_input      = true;  // 0x0115
    bool enable_warning_beep           = true;  // 0x011b
    bool left_shift_toggle_mode        = true;  // 0x0120
    bool right_shift_toggle_mode       = false; // 0x0121
    bool output_simplified             = false; // 0x012d (輸出字種: false=繁體字, true=简体字)
    CandidateOrientation orientation   = CandidateOrientation::Vertical; // 0x0145
};

struct KeyEvent {
    uint16_t vk_code      = 0;
    char16_t unicode_char = 0;
    uint16_t modifiers    = 0;
    bool     is_key_up    = false;
    uint64_t timestamp_ms = 0;
};

struct CandidateView {
    bool active           = false;
    bool is_associate     = false; // true: Shift+1..9 selects
    size_t total_count    = 0;
    size_t selected_index = 0;
    size_t page_start     = 0;
    std::vector<std::u16string> page_items;
};

struct EngineOutput {
    bool handled       = false;
    bool beep          = false;
    bool mode_changed  = false;
    bool symbol_keyboard_active = false;
    std::u16string commit_text;
    std::u16string composition;
    CandidateView candidates;
};

// 16-bit Bopomofo reading code helpers:
// [15:11] Initial (0..21) | [10:9] Medial (0..3) | [8:5] Final (0..13) | [4:2] Tone (0..5) | [1:0] 0
constexpr uint16_t EncodeReading(uint8_t initial, uint8_t medial, uint8_t final_v, uint8_t tone) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(initial & 0x1F) << 11) |
        (static_cast<uint16_t>(medial  & 0x03) << 9)  |
        (static_cast<uint16_t>(final_v & 0x0F) << 5)  |
        (static_cast<uint16_t>(tone    & 0x07) << 2));
}

constexpr uint16_t ReadingBucket(uint16_t reading_code) {
    return static_cast<uint16_t>(reading_code >> 2);
}

// Bopomofo Unicode display characters by slot value (0 = empty)
inline constexpr std::array<char16_t, 22> kInitialChars = {
    0,
    u'ㄅ', u'ㄆ', u'ㄇ', u'ㄈ', u'ㄉ', u'ㄊ', u'ㄋ', u'ㄌ',
    u'ㄍ', u'ㄎ', u'ㄏ', u'ㄐ', u'ㄑ', u'ㄒ', u'ㄓ', u'ㄔ',
    u'ㄕ', u'ㄖ', u'ㄗ', u'ㄘ', u'ㄙ'
};

inline constexpr std::array<char16_t, 4> kMedialChars = {
    0, u'ㄧ', u'ㄨ', u'ㄩ'
};

inline constexpr std::array<char16_t, 14> kFinalChars = {
    0,
    u'ㄚ', u'ㄛ', u'ㄜ', u'ㄝ', u'ㄞ', u'ㄟ', u'ㄠ',
    u'ㄡ', u'ㄢ', u'ㄣ', u'ㄤ', u'ㄥ', u'ㄦ'
};

inline constexpr std::array<char16_t, 6> kToneChars = {
    0, u' ', u'ˊ', u'ˇ', u'ˋ', u'˙'
};

} // namespace zzhuyin
