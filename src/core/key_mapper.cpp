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

} // namespace zzhuyin
