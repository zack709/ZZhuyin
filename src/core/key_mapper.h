#pragma once

#include "core/types.h"

#include <optional>

namespace zzhuyin {

struct BopomofoKey {
    BopomofoSlot slot;
    uint8_t value; // 1-based value within slot
};

class KeyMapper {
public:
    // Maps a lowercase ASCII key (or unshifted punctuation/digit/space) to a Bopomofo slot & value
    // according to the selected keyboard layout.
    // Returns std::nullopt if the key is not a Bopomofo key in that layout.
    // Note: Uppercase 'A'..'Z' returns std::nullopt (Windows 7 traditional Zhuyin treats
    // uppercase English letters as direct English output, not Bopomofo).
    static std::optional<BopomofoKey> MapBopomofo(KeyboardLayout layout, char16_t unicode_char);

    // Maps built-in Ctrl+ / Ctrl+Alt+ punctuation shortcuts (13 combinations).
    static std::optional<char16_t> MapPunctuationShortcut(uint16_t vk_code, uint16_t modifiers);

    // Maps non-Bopomofo ASCII punctuation to full-width Chinese punctuation (Phase 3 §5.3).
    static std::optional<char16_t> MapDirectFullWidthPunctuation(char16_t unicode_char);

    // Converts printable ASCII (0x20..0x7E) to full-width equivalent.
    static std::optional<char16_t> ToFullShapeAscii(char16_t unicode_char);

    // Maps VK code on the 47-key Win7 symbol soft keyboard (Ctrl+Alt+,) to full-width Unicode symbol.
    static std::optional<char16_t> MapSymbolKeyboard(uint16_t vk_code);
};

} // namespace zzhuyin
