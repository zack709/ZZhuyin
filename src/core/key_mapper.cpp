#include "core/key_mapper.h"

#include <array>

namespace zzhuyin {

namespace {

struct LayoutTable {
    std::array<char, 21> initials;
    std::array<char, 3>  medials;
    std::array<char, 13> finals;
    std::array<char, 5>  tones; // index 0..4 => tone 1..5
};

// All letters stored lowercase to match unshifted/non-caps ASCII input directly.
constexpr LayoutTable kStandardLayout = {
    {'1','q','a','z','2','w','s','x','e','d','c','r','f','v','5','t','g','b','y','h','n'},
    {'u','j','m'},
    {'8','i','k',',','9','o','l','.','0','p',';','/','-'},
    {' ','6','3','4','7'},
};

constexpr LayoutTable kEtenLayout = {
    {'b','p','m','f','d','t','n','l','v','k','h','g','7','c',',','.','/','j',';','\'','s'},
    {'e','x','u'},
    {'a','o','r','w','i','q','z','y','8','9','0','-','='},
    {' ','2','3','4','1'},
};

constexpr LayoutTable kIbmLayout = {
    {'1','2','3','4','5','6','7','8','9','0','-','q','w','e','r','t','y','u','i','o','p'},
    {'a','s','d'},
    {'f','g','h','j','k','l',';','z','x','c','v','b','n'},
    {' ','m',',','.','/'},
};

constexpr LayoutTable kGinYiehLayout = {
    {'2','w','s','x','3','e','d','c','r','f','v','t','g','b','6','y','h','n','u','j','m'},
    {'-','[','\''},
    {'8','i','k',',','9','o','l','.','0','p',';','/','='},
    {' ','q','a','z','1'},
};

const LayoutTable& GetLayoutTable(KeyboardLayout layout) {
    switch (layout) {
        case KeyboardLayout::Eten:    return kEtenLayout;
        case KeyboardLayout::Ibm:     return kIbmLayout;
        case KeyboardLayout::GinYieh: return kGinYiehLayout;
        case KeyboardLayout::Standard:
        default:                      return kStandardLayout;
    }
}

} // namespace

std::optional<BopomofoKey> KeyMapper::MapBopomofo(KeyboardLayout layout, char16_t unicode_char) {
    if (unicode_char < 0x20 || unicode_char > 0x7E) {
        return std::nullopt;
    }
    // Uppercase A..Z is treated as direct English output, not Bopomofo.
    if (unicode_char >= u'A' && unicode_char <= u'Z') {
        return std::nullopt;
    }

    const char ch = static_cast<char>(unicode_char);
    const LayoutTable& tbl = GetLayoutTable(layout);

    for (size_t i = 0; i < tbl.initials.size(); ++i) {
        if (tbl.initials[i] == ch) {
            return BopomofoKey{BopomofoSlot::Initial, static_cast<uint8_t>(i + 1)};
        }
    }
    for (size_t i = 0; i < tbl.medials.size(); ++i) {
        if (tbl.medials[i] == ch) {
            return BopomofoKey{BopomofoSlot::Medial, static_cast<uint8_t>(i + 1)};
        }
    }
    for (size_t i = 0; i < tbl.finals.size(); ++i) {
        if (tbl.finals[i] == ch) {
            return BopomofoKey{BopomofoSlot::Final, static_cast<uint8_t>(i + 1)};
        }
    }
    for (size_t i = 0; i < tbl.tones.size(); ++i) {
        if (tbl.tones[i] == ch) {
            return BopomofoKey{BopomofoSlot::Tone, static_cast<uint8_t>(i + 1)};
        }
    }
    return std::nullopt;
}

std::optional<char16_t> KeyMapper::MapPunctuationShortcut(uint16_t vk_code, uint16_t modifiers) {
    const uint16_t ctrl_alt_shift = static_cast<uint16_t>(modifiers & (mod::kShift | mod::kControl | mod::kAlt));

    if (ctrl_alt_shift == mod::kControl) {
        switch (vk_code) {
            case vk::kOemComma:  return u'，'; // U+FF0C
            case vk::kOemPeriod: return u'。'; // U+3002
            case vk::kOem1:      return u'；'; // U+FF1B
            case vk::kOem7:      return u'、'; // U+3001
            case vk::kOem4:      return u'【'; // U+3010
            case vk::kOem6:      return u'】'; // U+3011
            default:             return std::nullopt;
        }
    }

    if (ctrl_alt_shift == static_cast<uint16_t>(mod::kControl | mod::kAlt)) {
        switch (vk_code) {
            case '1':       return u'！'; // U+FF01
            case '9':       return u'｢';  // U+FF62
            case '0':       return u'｣';  // U+FF63
            case vk::kOem1: return u'：'; // U+FF1A
            case vk::kOem2: return u'？'; // U+FF1F
            case vk::kOem4: return u'｛'; // U+FF5B
            case vk::kOem6: return u'｝'; // U+FF5D
            default:        return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<char16_t> KeyMapper::MapDirectFullWidthPunctuation(char16_t unicode_char) {
    switch (unicode_char) {
        case u'!':  return u'！';
        case u'"':  return u'＂';
        case u'$':  return u'＄';
        case u'&':  return u'＆';
        case u'\'': return u'、';
        case u'(':  return u'（';
        case u')':  return u'）';
        case u',':  return u'，';
        case u'.':  return u'。';
        case u':':  return u'：';
        case u';':  return u'；';
        case u'<':  return u'＜';
        case u'>':  return u'＞';
        case u'?':  return u'？';
        case u'@':  return u'＠';
        case u'\\': return u'＼';
        case u'^':  return u'︿';
        case u'_':  return u'＿';
        case u'`':  return u'‵';
        default:    return std::nullopt;
    }
}

std::optional<char16_t> KeyMapper::ToFullShapeAscii(char16_t unicode_char) {
    if (unicode_char == u' ') {
        return u'\u3000';
    }
    if (unicode_char >= 0x21 && unicode_char <= 0x7E) {
        return static_cast<char16_t>(unicode_char - 0x21 + 0xFF01);
    }
    return std::nullopt;
}

std::optional<char16_t> KeyMapper::MapSymbolKeyboard(uint16_t vk_code) {
    switch (vk_code) {
        // Row 0 (14 keys)
        case vk::kOem3:      return u'\u20AC'; // ` -> €
        case '1':            return u'\u250C'; // 1 -> ┌
        case '2':            return u'\u252C'; // 2 -> ┬
        case '3':            return u'\u2510'; // 3 -> ┐
        case '4':            return u'\u301D'; // 4 -> 〝
        case '5':            return u'\u301E'; // 5 -> 〞
        case '6':            return u'\u2018'; // 6 -> ‘
        case '7':            return u'\u2019'; // 7 -> ’
        case '8':            return u'\u201C'; // 8 -> “
        case '9':            return u'\u201D'; // 9 -> ”
        case '0':            return u'\u300E'; // 0 -> 『
        case vk::kOemMinus:  return u'\u300F'; // - -> 』
        case vk::kOemPlus:   return u'\u300C'; // = -> 「
        case vk::kOem5:      return u'\u300D'; // \ -> 」

        // Row 1 (12 keys)
        case 'Q':            return u'\u251C'; // Q -> ├
        case 'W':            return u'\u253C'; // W -> ┼
        case 'E':            return u'\u2524'; // E -> ┤
        case 'R':            return u'\u203B'; // R -> ※
        case 'T':            return u'\uFF08'; // T -> （
        case 'Y':            return u'\uFF09'; // Y -> ）
        case 'U':            return u'\u300A'; // U -> 《
        case 'I':            return u'\u300B'; // I -> 》
        case 'O':            return u'\u3010'; // O -> 【
        case 'P':            return u'\u3011'; // P -> 】
        case vk::kOem4:      return u'\u3014'; // [ -> 〔
        case vk::kOem6:      return u'\u3015'; // ] -> 〕

        // Row 2 (11 keys)
        case 'A':            return u'\u2514'; // A -> └
        case 'S':            return u'\u2534'; // S -> ┴
        case 'D':            return u'\u2518'; // D -> ┘
        case 'F':            return u'\u25CB'; // F -> ○
        case 'G':            return u'\u25CF'; // G -> ●
        case 'H':            return u'\u2191'; // H -> ↑
        case 'J':            return u'\u2193'; // J -> ↓
        case 'K':            return u'\uFF01'; // K -> ！
        case 'L':            return u'\uFF1A'; // L -> ：
        case vk::kOem1:      return u'\uFF1B'; // ; -> ；
        case vk::kOem7:      return u'\u3001'; // ' -> 、

        // Row 3 (10 keys)
        case 'Z':            return u'\u2500'; // Z -> ─
        case 'X':            return u'\u2502'; // X -> │
        case 'C':            return u'\u25CE'; // C -> ◎
        case 'V':            return u'\u00A7'; // V -> §
        case 'B':            return u'\u2190'; // B -> ←
        case 'N':            return u'\u2192'; // N -> →
        case 'M':            return u'\u3002'; // M -> 。
        case vk::kOemComma:  return u'\uFF0C'; // , -> ，
        case vk::kOemPeriod: return u'\u2027'; // . -> ‧
        case vk::kOem2:      return u'\uFF1F'; // / -> ？

        default:             return std::nullopt;
    }
}

} // namespace zzhuyin
