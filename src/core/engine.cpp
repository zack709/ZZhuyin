#include "core/engine.h"

#include "core/key_mapper.h"

namespace zzhuyin {

namespace {

bool IsNumpadKey(uint16_t vk_code) {
    return vk_code >= vk::kNumpad0 && vk_code <= vk::kDivide;
}

bool IsDigit1To9(uint16_t vk_code) {
    return vk_code >= '1' && vk_code <= '9';
}

bool IsModifierKey(uint16_t vk_code) {
    switch (vk_code) {
        case vk::kShift:
        case vk::kLShift:
        case vk::kRShift:
        case vk::kControl:
        case vk::kLControl:
        case vk::kRControl:
        case vk::kMenu:
        case vk::kCapital:
            return true;
        default:
            return false;
    }
}

} // namespace

ZhuyinEngine::ZhuyinEngine(const IDictionary& dict, EngineConfig cfg)
    : dict_(&dict), cfg_(cfg), native_mode_(cfg.default_native_mode) {}

void ZhuyinEngine::SetConfig(EngineConfig cfg) {
    cfg_ = cfg;
}

void ZhuyinEngine::SetNativeMode(bool native) {
    if (native_mode_ != native) {
        native_mode_ = native;
        if (!native_mode_) {
            symbol_keyboard_active_ = false;
        }
        Reset();
    }
}

void ZhuyinEngine::SetFullShapeMode(bool full_shape) {
    full_shape_ = full_shape;
}

void ZhuyinEngine::SetSymbolKeyboardActive(bool active) {
    symbol_keyboard_active_ = active;
    if (active) {
        composer_.Clear();
        candidates_.Close();
    }
}

EngineState ZhuyinEngine::State() const {
    if (candidates_.IsActive()) {
        return candidates_.IsAssociate()
            ? EngineState::CandidateAssociate
            : EngineState::CandidateHomophone;
    }
    if (!composer_.IsEmpty()) {
        return EngineState::Composing;
    }
    return EngineState::Idle;
}

void ZhuyinEngine::Reset() {
    composer_.Clear();
    candidates_.Close();
    pending_shift_vk_ = 0;
    shift_interrupted_ = false;
}

bool ZhuyinEngine::WillHandleKey(const KeyEvent& ev) const {
    ZhuyinEngine copy = *this;
    return copy.ProcessKey(ev).handled;
}

void ZhuyinEngine::PopulateViewState(EngineOutput& out) const {
    if (!composer_.IsEmpty()) {
        out.composition = composer_.DisplayString();
    } else if (candidates_.IsActive() && !candidates_.IsAssociate()) {
        // Preview currently highlighted (default #1) candidate in composition while selecting
        out.composition = candidates_.SelectCurrent().value_or(std::u16string{});
    } else {
        out.composition.clear();
    }
    out.candidates = candidates_.View();
    out.symbol_keyboard_active = symbol_keyboard_active_;
}

bool ZhuyinEngine::HandleShiftShortPress(const KeyEvent& ev, EngineOutput& out) {
    const bool is_lshift = (ev.vk_code == vk::kLShift);
    const bool is_rshift = (ev.vk_code == vk::kRShift);
    const bool is_tracked_shift =
        (is_lshift && cfg_.left_shift_toggle_mode) ||
        (is_rshift && cfg_.right_shift_toggle_mode);

    if (!ev.is_key_up) {
        if (is_tracked_shift && (ev.modifiers & (mod::kControl | mod::kAlt)) == 0) {
            if (pending_shift_vk_ != ev.vk_code) {
                pending_shift_vk_ = ev.vk_code;
                pending_shift_down_ms_ = ev.timestamp_ms;
                shift_interrupted_ = false;
            }
            // Do not swallow Shift KeyDown so Shift+key combos still work
            return false;
        }
        // Any other key pressed interrupts a pending Shift short-press
        if (pending_shift_vk_ != 0 && ev.vk_code != pending_shift_vk_) {
            shift_interrupted_ = true;
        }
        return false;
    }

    // KeyUp
    if (pending_shift_vk_ != 0 && ev.vk_code == pending_shift_vk_) {
        const bool ok_time =
            (ev.timestamp_ms >= pending_shift_down_ms_) &&
            ((ev.timestamp_ms - pending_shift_down_ms_) < kShiftShortPressMaxMs);
        const bool should_toggle = !shift_interrupted_ && ok_time;
        pending_shift_vk_ = 0;
        shift_interrupted_ = false;

        if (should_toggle) {
            native_mode_ = !native_mode_;
            if (!native_mode_) {
                symbol_keyboard_active_ = false;
            }
            composer_.Clear();
            candidates_.Close();
            out.handled = true;
            out.mode_changed = true;
            return true;
        }
    }
    return false;
}

void ZhuyinEngine::CommitAndMaybeAssociate(
    const std::u16string& text, bool allow_associate, EngineOutput& out) {
    out.commit_text += text;
    candidates_.Close();
    if (allow_associate && cfg_.prompt_associate_phrase && !text.empty()) {
        auto assoc = dict_->LookupAssociate(text[0]);
        if (!assoc.empty()) {
            candidates_.OpenAssociate(std::move(assoc));
        }
    }
}

bool ZhuyinEngine::HandleCandidateKey(const KeyEvent& ev, EngineOutput& out) {
    if (!candidates_.IsActive()) {
        return false;
    }
    if (IsModifierKey(ev.vk_code)) {
        return false;
    }

    const bool has_ctrl  = (ev.modifiers & mod::kControl) != 0;
    const bool has_alt   = (ev.modifiers & mod::kAlt) != 0;
    const bool has_shift = (ev.modifiers & mod::kShift) != 0;

    if (!has_ctrl && !has_alt) {
        if (!has_shift && (ev.vk_code == vk::kEscape || ev.vk_code == vk::kBackspace)) {
            if (candidates_.IsAssociate() && ev.vk_code == vk::kBackspace) {
                candidates_.Close();
                return false;
            }
            candidates_.Close();
            composer_.Clear();
            out.handled = true;
            return true;
        }

        if (ev.vk_code == vk::kReturn) {
            if (auto sel = candidates_.SelectCurrent()) {
                const bool was_homophone = !candidates_.IsAssociate();
                CommitAndMaybeAssociate(*sel, was_homophone, out);
                out.handled = true;
                return true;
            }
            candidates_.Close();
            return false;
        }

        if (ev.vk_code == vk::kSpace || ev.vk_code == vk::kPageDown) {
            candidates_.PageDown();
            out.handled = true;
            return true;
        }

        if (ev.vk_code == vk::kPageUp) {
            candidates_.PageUp();
            out.handled = true;
            return true;
        }

        if (ev.vk_code == vk::kUp || ev.vk_code == vk::kDown ||
            ev.vk_code == vk::kLeft || ev.vk_code == vk::kRight) {
            uint16_t effective_vk = ev.vk_code;
            if (cfg_.orientation == CandidateOrientation::Horizontal) {
                switch (ev.vk_code) {
                    case vk::kLeft:  effective_vk = vk::kUp;    break;
                    case vk::kRight: effective_vk = vk::kDown;  break;
                    case vk::kUp:    effective_vk = vk::kLeft;  break;
                    case vk::kDown:  effective_vk = vk::kRight; break;
                    default: break;
                }
            }
            if (effective_vk == vk::kUp) {
                candidates_.MoveUp();
            } else if (effective_vk == vk::kDown) {
                candidates_.MoveDown();
            }
            // Traditional mode intercepts Left/Right with no-op
            out.handled = true;
            return true;
        }

        if (ev.vk_code == vk::kTab || ev.vk_code == vk::kDelete) {
            out.handled = true;
            return true;
        }

        if (IsDigit1To9(ev.vk_code)) {
            const size_t offset = static_cast<size_t>(ev.vk_code - '1');
            if (!candidates_.IsAssociate() && !has_shift) {
                if (auto sel = candidates_.SelectOnPage(offset)) {
                    CommitAndMaybeAssociate(*sel, /*allow_associate=*/true, out);
                }
                out.handled = true;
                return true;
            }
            if (candidates_.IsAssociate() && has_shift) {
                if (auto sel = candidates_.SelectOnPage(offset)) {
                    CommitAndMaybeAssociate(*sel, /*allow_associate=*/false, out);
                }
                out.handled = true;
                return true;
            }
            // Bare 1..9 in associate mode (or Shift+1..9 in homophone mode):
            // close candidate window and fall through to composer/punctuation.
        }
    }

    // Unhandled key in candidate mode:
    // If in homophone selection, auto-commit the currently highlighted (default #1) candidate
    // before falling through to process the new key.
    if (!candidates_.IsAssociate()) {
        if (auto sel = candidates_.SelectCurrent()) {
            out.commit_text += *sel;
        }
    }
    candidates_.Close();
    return false;
}

void ZhuyinEngine::TriggerLookup(uint8_t tone, EngineOutput& out) {
    const uint16_t code = composer_.Encode(tone);
    auto cands = dict_->LookupReading(code, cfg_.charset_filter);

    if (cands.empty()) {
        // Invalid Bopomofo combination: beep + revert tone only, keep syllable body
        if (cfg_.enable_warning_beep) {
            out.beep = true;
        }
        composer_.ClearTone();
        out.handled = true;
        return;
    }

    composer_.Clear();

    if (cands.size() == 1) {
        // Traditional mode unconditionally auto-commits single candidate
        CommitAndMaybeAssociate(cands[0], /*allow_associate=*/true, out);
        out.handled = true;
        return;
    }

    candidates_.OpenHomophone(std::move(cands));
    out.handled = true;
}

bool ZhuyinEngine::HandleComposerKey(const KeyEvent& ev, EngineOutput& out) {
    const bool has_ctrl = (ev.modifiers & mod::kControl) != 0;
    const bool has_alt  = (ev.modifiers & mod::kAlt) != 0;

    if (has_ctrl || has_alt) {
        return false;
    }

    // Numpad keys: clear composition if composing, then pass through
    if (IsNumpadKey(ev.vk_code)) {
        if (!composer_.IsEmpty()) {
            composer_.Clear();
        }
        return false;
    }

    // Editing / control keys while composing
    if (!composer_.IsEmpty()) {
        switch (ev.vk_code) {
            case vk::kBackspace:
                composer_.Backspace();
                out.handled = true;
                return true;
            case vk::kEscape:
            case vk::kDelete:
                composer_.Clear();
                out.handled = true;
                return true;
            case vk::kReturn:
                TriggerLookup(/*tone=*/1, out);
                return true;
            case vk::kUp:
            case vk::kDown:
            case vk::kLeft:
            case vk::kRight:
            case vk::kPageUp:
            case vk::kPageDown:
                out.handled = true;
                return true;
            default:
                break;
        }
    }

    // Uppercase English (via Shift or CapsLock inversion): clear composing & pass through
    if (ev.unicode_char >= u'A' && ev.unicode_char <= u'Z') {
        if (!composer_.IsEmpty()) {
            composer_.Clear();
        }
        if (full_shape_) {
            if (auto fw = KeyMapper::ToFullShapeAscii(ev.unicode_char)) {
                out.commit_text.push_back(*fw);
                out.handled = true;
                return true;
            }
        }
        return false;
    }

    // Bopomofo mapping
    if (auto mapped = KeyMapper::MapBopomofo(cfg_.layout, ev.unicode_char)) {
        if (mapped->slot == BopomofoSlot::Tone) {
            if (!composer_.HasSyllableBody()) {
                if (ev.vk_code == vk::kSpace) {
                    if (full_shape_) {
                        out.commit_text.push_back(u'\u3000');
                        out.handled = true;
                        return true;
                    }
                    return false; // Pass through half-width space in IDLE
                }
                // Tone key pressed with no syllable body -> ignore
                out.handled = true;
                return true;
            }
            TriggerLookup(mapped->value, out);
            return true;
        }

        composer_.Input(mapped->slot, mapped->value);
        out.handled = true;
        return true;
    }

    return false;
}

EngineOutput ZhuyinEngine::ProcessKey(const KeyEvent& ev) {
    EngineOutput out;

    // Priority 1a: Shift short-press mode toggle
    if (HandleShiftShortPress(ev, out)) {
        PopulateViewState(out);
        return out;
    }

    // Ignore all other KeyUp events
    if (ev.is_key_up) {
        PopulateViewState(out);
        return out;
    }

    // Priority 1a-2: Ctrl + Space toggles Native (Chinese) / English mode
    const uint16_t ctrl_alt_shift =
        static_cast<uint16_t>(ev.modifiers & (mod::kShift | mod::kControl | mod::kAlt));
    if (ev.vk_code == vk::kSpace && ctrl_alt_shift == mod::kControl) {
        if (candidates_.IsActive() && !candidates_.IsAssociate()) {
            if (auto sel = candidates_.SelectCurrent()) {
                out.commit_text += *sel;
            }
        }
        native_mode_ = !native_mode_;
        if (!native_mode_) {
            symbol_keyboard_active_ = false;
        }
        composer_.Clear();
        candidates_.Close();
        out.handled = true;
        out.mode_changed = true;
        PopulateViewState(out);
        return out;
    }

    // English mode
    if (!native_mode_) {
        const bool has_ctrl_alt = (ev.modifiers & (mod::kControl | mod::kAlt)) != 0;
        if (full_shape_ && !has_ctrl_alt && !IsNumpadKey(ev.vk_code)) {
            if (auto fw = KeyMapper::ToFullShapeAscii(ev.unicode_char)) {
                out.commit_text.push_back(*fw);
                out.handled = true;
            }
        }
        PopulateViewState(out);
        return out;
    }

    // Priority 1b: Ctrl + Alt + , toggles the 47-key full-width symbol keyboard
    if (ev.vk_code == vk::kOemComma &&
        ctrl_alt_shift == static_cast<uint16_t>(mod::kControl | mod::kAlt)) {
        if (candidates_.IsActive() && !candidates_.IsAssociate()) {
            if (auto sel = candidates_.SelectCurrent()) {
                out.commit_text += *sel;
            }
        }
        composer_.Clear();
        candidates_.Close();
        symbol_keyboard_active_ = !symbol_keyboard_active_;
        out.handled = true;
        PopulateViewState(out);
        return out;
    }

    // Priority 1c: Symbol soft keyboard active (Phase 3 Handler #2)
    if (symbol_keyboard_active_) {
        const bool has_ctrl_alt = (ev.modifiers & (mod::kControl | mod::kAlt)) != 0;
        if (!has_ctrl_alt) {
            if (ev.vk_code == vk::kEscape) {
                symbol_keyboard_active_ = false;
                out.handled = true;
                PopulateViewState(out);
                return out;
            }
            if (auto sym = KeyMapper::MapSymbolKeyboard(ev.vk_code)) {
                out.commit_text.push_back(*sym);
                symbol_keyboard_active_ = false;
                out.handled = true;
                PopulateViewState(out);
                return out;
            }
        }
        PopulateViewState(out);
        return out;
    }

    // Priority 1d: Built-in punctuation shortcuts (Ctrl+, / Ctrl+Alt+...)
    if (cfg_.enable_punctuation_input) {
        if (auto punc = KeyMapper::MapPunctuationShortcut(ev.vk_code, ev.modifiers)) {
            if (candidates_.IsActive() && !candidates_.IsAssociate()) {
                if (auto sel = candidates_.SelectCurrent()) {
                    out.commit_text += *sel;
                }
            }
            composer_.Clear();
            candidates_.Close();
            out.commit_text.push_back(*punc);
            out.handled = true;
            PopulateViewState(out);
            return out;
        }
    }

    // Priority 2: Candidate window
    if (HandleCandidateKey(ev, out)) {
        PopulateViewState(out);
        return out;
    }

    // Priority 3: Bopomofo composer
    if (HandleComposerKey(ev, out)) {
        PopulateViewState(out);
        return out;
    }

    // Priority 4: Direct full-width punctuation & full-shape ASCII (Numpad always stays half-width)
    const bool has_ctrl_alt = (ev.modifiers & (mod::kControl | mod::kAlt)) != 0;
    if (!has_ctrl_alt) {
        const bool is_numpad = IsNumpadKey(ev.vk_code);
        if (cfg_.enable_punctuation_input && !is_numpad) {
            if (auto punc = KeyMapper::MapDirectFullWidthPunctuation(ev.unicode_char)) {
                composer_.Clear();
                out.commit_text.push_back(*punc);
                out.handled = true;
                PopulateViewState(out);
                return out;
            }
        }
        if (full_shape_ && !is_numpad) {
            if (auto fw = KeyMapper::ToFullShapeAscii(ev.unicode_char)) {
                composer_.Clear();
                out.commit_text.push_back(*fw);
                out.handled = true;
                PopulateViewState(out);
                return out;
            }
        }
        // If a pending #1 candidate was auto-committed on a pass-through printable key (e.g. 'A'),
        // commit both the candidate and the printable key so TSF eats and applies the event.
        if (!out.commit_text.empty() && ev.unicode_char >= 0x20) {
            out.commit_text.push_back(ev.unicode_char);
            out.handled = true;
        }
    }

    if (!out.commit_text.empty()) {
        out.handled = true;
    }

    PopulateViewState(out);
    return out;
}

} // namespace zzhuyin
