#pragma once

#include "core/types.h"

#include <array>
#include <string>

namespace zzhuyin {

class BopomofoComposer {
public:
    void Input(BopomofoSlot slot, uint8_t value);
    bool Backspace();
    void ClearTone();
    void Clear();

    bool IsEmpty() const;
    bool HasSyllableBody() const; // Initial, Medial, or Final is non-empty

    uint16_t Encode(uint8_t tone_override = 0) const;
    std::u16string DisplayString() const;

    uint8_t SlotValue(BopomofoSlot slot) const;

private:
    std::array<uint8_t, 4> slots_ = {0, 0, 0, 0};
};

} // namespace zzhuyin
