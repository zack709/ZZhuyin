#include "core/composer.h"

namespace zzhuyin {

void BopomofoComposer::Input(BopomofoSlot slot, uint8_t value) {
    slots_[static_cast<size_t>(slot)] = value;
}

bool BopomofoComposer::Backspace() {
    for (int i = 3; i >= 0; --i) {
        if (slots_[static_cast<size_t>(i)] != 0) {
            slots_[static_cast<size_t>(i)] = 0;
            return true;
        }
    }
    return false;
}

void BopomofoComposer::ClearTone() {
    slots_[3] = 0;
}

void BopomofoComposer::Clear() {
    slots_ = {0, 0, 0, 0};
}

bool BopomofoComposer::IsEmpty() const {
    return slots_[0] == 0 && slots_[1] == 0 && slots_[2] == 0 && slots_[3] == 0;
}

bool BopomofoComposer::HasSyllableBody() const {
    return slots_[0] != 0 || slots_[1] != 0 || slots_[2] != 0;
}

uint16_t BopomofoComposer::Encode(uint8_t tone_override) const {
    uint8_t tone = (tone_override != 0) ? tone_override : slots_[3];
    if (tone == 0 && HasSyllableBody()) {
        tone = 1; // Auto tone-1 (一聲)
    }
    return EncodeReading(slots_[0], slots_[1], slots_[2], tone);
}

std::u16string BopomofoComposer::DisplayString() const {
    std::u16string out;
    out.reserve(4);
    if (slots_[0] > 0 && slots_[0] < kInitialChars.size()) {
        out.push_back(kInitialChars[slots_[0]]);
    }
    if (slots_[1] > 0 && slots_[1] < kMedialChars.size()) {
        out.push_back(kMedialChars[slots_[1]]);
    }
    if (slots_[2] > 0 && slots_[2] < kFinalChars.size()) {
        out.push_back(kFinalChars[slots_[2]]);
    }
    if (slots_[3] > 1 && slots_[3] < kToneChars.size()) {
        // Tone 1 (space) is typically not displayed as a symbol in composition
        out.push_back(kToneChars[slots_[3]]);
    }
    return out;
}

uint8_t BopomofoComposer::SlotValue(BopomofoSlot slot) const {
    return slots_[static_cast<size_t>(slot)];
}

} // namespace zzhuyin
