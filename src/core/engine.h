#pragma once

#include "core/candidate.h"
#include "core/composer.h"
#include "core/dictionary.h"
#include "core/types.h"

namespace zzhuyin {

class ZhuyinEngine {
public:
    static constexpr uint64_t kShiftShortPressMaxMs = 401;

    ZhuyinEngine(const IDictionary& dict, EngineConfig cfg = {});

    // Pure dry-run query for TSF OnTestKeyDown / OnTestKeyUp (does not mutate state).
    bool WillHandleKey(const KeyEvent& ev) const;

    // Processes a key event and advances the state machine.
    EngineOutput ProcessKey(const KeyEvent& ev);

    void SetConfig(EngineConfig cfg);
    const EngineConfig& Config() const { return cfg_; }

    void SetNativeMode(bool native);
    void SetFullShapeMode(bool full_shape);
    bool IsNativeMode() const { return native_mode_; }
    bool IsFullShapeMode() const { return full_shape_; }

    EngineState State() const;
    void Reset();

private:
    bool HandleShiftShortPress(const KeyEvent& ev, EngineOutput& out);
    bool HandleCandidateKey(const KeyEvent& ev, EngineOutput& out);
    bool HandleComposerKey(const KeyEvent& ev, EngineOutput& out);
    void TriggerLookup(uint8_t tone, EngineOutput& out);
    void CommitAndMaybeAssociate(const std::u16string& text, bool allow_associate, EngineOutput& out);
    void PopulateViewState(EngineOutput& out) const;

    const IDictionary* dict_;
    EngineConfig cfg_;

    bool native_mode_ = true;
    bool full_shape_ = false;

    BopomofoComposer composer_;
    CandidateManager candidates_;

    // Shift short-press tracking
    uint16_t pending_shift_vk_ = 0;
    uint64_t pending_shift_down_ms_ = 0;
    bool shift_interrupted_ = false;
};

} // namespace zzhuyin
