#include "test_runner.h"

#include "core/engine.h"
#include "core/imechlex_dict.h"

#include <filesystem>

namespace {

const zzhuyin::ImechlexDictionary& TestDict() {
    static zzhuyin::ImechlexDictionary dict;
    static bool loaded = false;
    if (!loaded) {
        auto root = std::filesystem::path(ZZHUYIN_SOURCE_DIR) / "targets";
        dict.LoadImechlex(root / "DICTS/IMTCPH.IMD");
        dict.LoadAssociateTables(root / "lcptr.tbl", root / "lcphrase.tbl");
        loaded = true;
    }
    return dict;
}

zzhuyin::KeyEvent MakeCharKey(char ch, uint16_t vk_override = 0, uint16_t mods = 0, uint64_t ts = 0) {
    zzhuyin::KeyEvent ev;
    ev.unicode_char = static_cast<char16_t>(ch);
    ev.modifiers = mods;
    ev.timestamp_ms = ts;
    if (vk_override != 0) {
        ev.vk_code = vk_override;
    } else if (ch >= 'a' && ch <= 'z') {
        ev.vk_code = static_cast<uint16_t>(ch - 'a' + 'A');
    } else if (ch >= 'A' && ch <= 'Z') {
        ev.vk_code = static_cast<uint16_t>(ch);
    } else if (ch >= '0' && ch <= '9') {
        ev.vk_code = static_cast<uint16_t>(ch);
    } else if (ch == ' ') {
        ev.vk_code = zzhuyin::vk::kSpace;
    } else if (ch == ',') {
        ev.vk_code = zzhuyin::vk::kOemComma;
    } else if (ch == '.') {
        ev.vk_code = zzhuyin::vk::kOemPeriod;
    } else if (ch == ';') {
        ev.vk_code = zzhuyin::vk::kOem1;
    } else if (ch == '/') {
        ev.vk_code = zzhuyin::vk::kOem2;
    } else if (ch == '-') {
        ev.vk_code = zzhuyin::vk::kOemMinus;
    } else if (ch == '=') {
        ev.vk_code = zzhuyin::vk::kOemPlus;
    } else if (ch == '[') {
        ev.vk_code = zzhuyin::vk::kOem4;
    } else if (ch == '\'') {
        ev.vk_code = zzhuyin::vk::kOem7;
    }
    return ev;
}

zzhuyin::KeyEvent MakeVkKey(uint16_t vk_code, uint16_t mods = 0, bool is_up = false, uint64_t ts = 0) {
    zzhuyin::KeyEvent ev;
    ev.vk_code = vk_code;
    ev.modifiers = mods;
    ev.is_key_up = is_up;
    ev.timestamp_ms = ts;
    return ev;
}

} // namespace

TEST(T3_1_InvalidCombinationBeepAndRevertTone) {
    zzhuyin::ZhuyinEngine engine(TestDict());

    // 1 (ㄅ) -> m (ㄩ) -> 4 (ˋ) : invalid syllable ㄅㄩˋ
    auto o1 = engine.ProcessKey(MakeCharKey('1'));
    EXPECT_TRUE(o1.handled);
    EXPECT_EQ(o1.composition, u"ㄅ");

    auto o2 = engine.ProcessKey(MakeCharKey('m'));
    EXPECT_TRUE(o2.handled);
    EXPECT_EQ(o2.composition, u"ㄅㄩ");

    auto o3 = engine.ProcessKey(MakeCharKey('4'));
    EXPECT_TRUE(o3.handled);
    EXPECT_TRUE(o3.beep);
    EXPECT_EQ(o3.composition, u"ㄅㄩ"); // Tone reverted, body kept
    EXPECT_TRUE(o3.commit_text.empty());
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Composing);
}

TEST(T4_1_SingleCandidateAutoCommitNoAssociate) {
    zzhuyin::EngineConfig cfg;
    cfg.prompt_associate_phrase = false;
    zzhuyin::ZhuyinEngine engine(TestDict(), cfg);

    // ㄅ(1) ㄞ(9) ˊ(6) -> only 1 candidate: 白
    engine.ProcessKey(MakeCharKey('1'));
    engine.ProcessKey(MakeCharKey('9'));
    auto out = engine.ProcessKey(MakeCharKey('6'));

    EXPECT_TRUE(out.handled);
    EXPECT_EQ(out.commit_text, u"白");
    EXPECT_FALSE(out.candidates.active);
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Idle);
}

TEST(T4_2_SingleCandidateAutoCommitWithAssociate) {
    zzhuyin::EngineConfig cfg;
    cfg.prompt_associate_phrase = true;
    zzhuyin::ZhuyinEngine engine(TestDict(), cfg);

    // ㄅ(1) ㄞ(9) ˊ(6) -> auto-commits 白 and opens associate candidates
    engine.ProcessKey(MakeCharKey('1'));
    engine.ProcessKey(MakeCharKey('9'));
    auto out = engine.ProcessKey(MakeCharKey('6'));

    EXPECT_TRUE(out.handled);
    EXPECT_EQ(out.commit_text, u"白");
    EXPECT_TRUE(out.candidates.active);
    EXPECT_TRUE(out.candidates.is_associate);
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::CandidateAssociate);
}

TEST(T5_6_DigitSelectAndEscCancel) {
    zzhuyin::ZhuyinEngine engine(TestDict());

    // ㄉ(2) ㄜ(k) ˙(7) -> 的, 得, 地
    engine.ProcessKey(MakeCharKey('2'));
    engine.ProcessKey(MakeCharKey('k'));
    auto o_cand = engine.ProcessKey(MakeCharKey('7'));
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::CandidateHomophone);
    EXPECT_EQ(o_cand.candidates.total_count, 3u);

    // Press '2' -> commit 得
    auto o_sel = engine.ProcessKey(MakeCharKey('2'));
    EXPECT_TRUE(o_sel.handled);
    EXPECT_EQ(o_sel.commit_text, u"得");
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Idle);

    // Trigger again and cancel with Esc
    engine.ProcessKey(MakeCharKey('2'));
    engine.ProcessKey(MakeCharKey('k'));
    engine.ProcessKey(MakeCharKey('7'));
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::CandidateHomophone);

    auto o_esc = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kEscape));
    EXPECT_TRUE(o_esc.handled);
    EXPECT_TRUE(o_esc.commit_text.empty());
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Idle);
}

TEST(T6_1_And_T6_2_AssociatePhraseSelectionVsTyping) {
    zzhuyin::EngineConfig cfg;
    cfg.prompt_associate_phrase = true;
    zzhuyin::ZhuyinEngine engine(TestDict(), cfg);

    // ㄅㄞˊ -> 白 + associate phrases
    engine.ProcessKey(MakeCharKey('1'));
    engine.ProcessKey(MakeCharKey('9'));
    auto o_assoc = engine.ProcessKey(MakeCharKey('6'));
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::CandidateAssociate);
    const std::u16string expected_first = o_assoc.candidates.page_items[0];

    // T6-1: Shift+1 selects first associate phrase
    auto o_shift1 = engine.ProcessKey(MakeCharKey('!', '1', zzhuyin::mod::kShift));
    EXPECT_TRUE(o_shift1.handled);
    EXPECT_EQ(o_shift1.commit_text, expected_first);
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Idle);

    // Trigger associate again
    engine.ProcessKey(MakeCharKey('1'));
    engine.ProcessKey(MakeCharKey('9'));
    engine.ProcessKey(MakeCharKey('6'));
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::CandidateAssociate);

    // T6-2: Bare '1' closes associate window and starts composing ㄅ
    auto o_bare1 = engine.ProcessKey(MakeCharKey('1'));
    EXPECT_TRUE(o_bare1.handled);
    EXPECT_TRUE(o_bare1.commit_text.empty());
    EXPECT_EQ(o_bare1.composition, u"ㄅ");
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Composing);
}

TEST(T7_1_BackspaceRemovesHighestSlot) {
    zzhuyin::ZhuyinEngine engine(TestDict());

    // ㄓ(5) ㄨ(j) ㄥ(/)
    engine.ProcessKey(MakeCharKey('5'));
    engine.ProcessKey(MakeCharKey('j'));
    engine.ProcessKey(MakeCharKey('/'));
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Composing);

    auto b1 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kBackspace));
    EXPECT_EQ(b1.composition, u"ㄓㄨ");
    auto b2 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kBackspace));
    EXPECT_EQ(b2.composition, u"ㄓ");
    auto b3 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kBackspace));
    EXPECT_EQ(b3.composition, u"");
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Idle);
}

TEST(T7_2_EnterAutoTone1) {
    zzhuyin::ZhuyinEngine engine(TestDict());

    // ㄓ(5) ㄨ(j) ㄥ(/) + Enter -> lookup ㄓㄨㄥ (tone 1), first candidate 中
    engine.ProcessKey(MakeCharKey('5'));
    engine.ProcessKey(MakeCharKey('j'));
    engine.ProcessKey(MakeCharKey('/'));
    auto out = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kReturn));

    EXPECT_TRUE(out.handled);
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::CandidateHomophone);
    EXPECT_TRUE(!out.candidates.page_items.empty());
    EXPECT_EQ(out.candidates.page_items[0], u"中");
}

TEST(T7_3_CapsLockInversion) {
    zzhuyin::ZhuyinEngine engine(TestDict());

    // CapsLock ON, unshifted 'a' -> ToUnicode gives 'A' -> pass through uppercase English
    auto o_upper = engine.ProcessKey(MakeCharKey('A', 'A', zzhuyin::mod::kCapsLock));
    EXPECT_FALSE(o_upper.handled);
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Idle);

    // CapsLock ON + Shift + 'a' -> ToUnicode gives 'a' -> enters Bopomofo ㄇ
    auto o_lower = engine.ProcessKey(
        MakeCharKey('a', 'A', static_cast<uint16_t>(zzhuyin::mod::kCapsLock | zzhuyin::mod::kShift)));
    EXPECT_TRUE(o_lower.handled);
    EXPECT_EQ(o_lower.composition, u"ㄇ");
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Composing);
}

TEST(T8_1_And_T8_2_LeftShiftShortPress) {
    zzhuyin::ZhuyinEngine engine(TestDict());
    EXPECT_TRUE(engine.IsNativeMode());

    // T8-1: Short press (200 ms < 401 ms) toggles native -> English
    auto down1 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kLShift, zzhuyin::mod::kShift, false, 1000));
    EXPECT_FALSE(down1.handled);
    auto up1 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kLShift, 0, true, 1200));
    EXPECT_TRUE(up1.handled);
    EXPECT_TRUE(up1.mode_changed);
    EXPECT_FALSE(engine.IsNativeMode());

    // T8-2a: Long press (500 ms >= 401 ms) does NOT toggle
    engine.ProcessKey(MakeVkKey(zzhuyin::vk::kLShift, zzhuyin::mod::kShift, false, 2000));
    auto up2 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kLShift, 0, true, 2500));
    EXPECT_FALSE(up2.mode_changed);
    EXPECT_FALSE(engine.IsNativeMode());

    // T8-2b: Shift + A (interrupted) does NOT toggle
    engine.ProcessKey(MakeVkKey(zzhuyin::vk::kLShift, zzhuyin::mod::kShift, false, 3000));
    engine.ProcessKey(MakeCharKey('A', 'A', zzhuyin::mod::kShift, 3050));
    auto up3 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kLShift, 0, true, 3100));
    EXPECT_FALSE(up3.mode_changed);
    EXPECT_FALSE(engine.IsNativeMode());
}

TEST(T8_3_PunctuationShortcutsAndDirectFullWidth) {
    zzhuyin::ZhuyinEngine engine(TestDict());

    auto o_comma = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kOemComma, zzhuyin::mod::kControl));
    EXPECT_TRUE(o_comma.handled);
    EXPECT_EQ(o_comma.commit_text, u"，");

    auto o_period = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kOemPeriod, zzhuyin::mod::kControl));
    EXPECT_TRUE(o_period.handled);
    EXPECT_EQ(o_period.commit_text, u"。");

    auto o_excl = engine.ProcessKey(MakeCharKey('!', '1', zzhuyin::mod::kShift));
    EXPECT_TRUE(o_excl.handled);
    EXPECT_EQ(o_excl.commit_text, u"！");
}

TEST(T9_1_MultiKeyboardLayouts) {
    // ETen: d(ㄉ) r(ㄜ) 1(˙) -> 的, 得, 地
    {
        zzhuyin::EngineConfig cfg;
        cfg.layout = zzhuyin::KeyboardLayout::Eten;
        zzhuyin::ZhuyinEngine engine(TestDict(), cfg);

        engine.ProcessKey(MakeCharKey('d'));
        engine.ProcessKey(MakeCharKey('r'));
        auto out = engine.ProcessKey(MakeCharKey('1'));
        EXPECT_EQ(out.candidates.total_count, 3u);
        EXPECT_EQ(out.candidates.page_items[0], u"的");
        EXPECT_EQ(out.candidates.page_items[1], u"得");
        EXPECT_EQ(out.candidates.page_items[2], u"地");
    }

    // IBM: 5(ㄉ) h(ㄜ) /(˙) -> 的, 得, 地
    {
        zzhuyin::EngineConfig cfg;
        cfg.layout = zzhuyin::KeyboardLayout::Ibm;
        zzhuyin::ZhuyinEngine engine(TestDict(), cfg);

        engine.ProcessKey(MakeCharKey('5'));
        engine.ProcessKey(MakeCharKey('h'));
        auto out = engine.ProcessKey(MakeCharKey('/'));
        EXPECT_EQ(out.candidates.total_count, 3u);
        EXPECT_EQ(out.candidates.page_items[0], u"的");
        EXPECT_EQ(out.candidates.page_items[1], u"得");
        EXPECT_EQ(out.candidates.page_items[2], u"地");
    }
}

TEST(DryRunWillHandleKeyDoesNotMutate) {
    zzhuyin::ZhuyinEngine engine(TestDict());
    EXPECT_TRUE(engine.WillHandleKey(MakeCharKey('1')));
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Idle);
}

TEST(CtrlSpaceTogglesNativeAndEnglishMode) {
    zzhuyin::ZhuyinEngine engine(TestDict());
    EXPECT_TRUE(engine.IsNativeMode());

    // Start composing, then press Ctrl + Space -> clears composition and switches to English
    engine.ProcessKey(MakeCharKey('1'));
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Composing);

    auto o1 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kSpace, zzhuyin::mod::kControl));
    EXPECT_TRUE(o1.handled);
    EXPECT_TRUE(o1.mode_changed);
    EXPECT_FALSE(engine.IsNativeMode());
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Idle);

    // Press Ctrl + Space again in English mode -> switches back to Chinese
    auto o2 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kSpace, zzhuyin::mod::kControl));
    EXPECT_TRUE(o2.handled);
    EXPECT_TRUE(o2.mode_changed);
    EXPECT_TRUE(engine.IsNativeMode());
}

TEST(DefaultSelectFirstCandidateWhenContinuingTyping) {
    zzhuyin::ZhuyinEngine engine(TestDict());

    // Type ㄓ(5) ㄨ(j) ˋ(4) -> candidate window opens, #1 is 住 (previewed in composition)
    engine.ProcessKey(MakeCharKey('5'));
    engine.ProcessKey(MakeCharKey('j'));
    auto o_cand = engine.ProcessKey(MakeCharKey('4'));
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::CandidateHomophone);
    EXPECT_EQ(o_cand.composition, u"住");

    // Without pressing 1..9, directly type next syllable ㄧ(u) -> auto-commits #1 住 and composes ㄧ
    auto o_next = engine.ProcessKey(MakeCharKey('u'));
    EXPECT_TRUE(o_next.handled);
    EXPECT_EQ(o_next.commit_text, u"住");
    EXPECT_EQ(o_next.composition, u"ㄧ");
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Composing);

    // Finish ㄧㄣ(p + Space) -> candidate window opens (#1 is 因), then press Ctrl+, -> commits 因，
    engine.ProcessKey(MakeCharKey('p'));
    auto o_yin = engine.ProcessKey(MakeCharKey(' '));
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::CandidateHomophone);
    EXPECT_EQ(o_yin.composition, u"因");

    auto o_punc = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kOemComma, zzhuyin::mod::kControl));
    EXPECT_TRUE(o_punc.handled);
    EXPECT_EQ(o_punc.commit_text, u"因，");
    EXPECT_EQ(engine.State(), zzhuyin::EngineState::Idle);
}

TEST(SymbolKeyboardCtrlAltComma) {
    zzhuyin::ZhuyinEngine engine(TestDict());
    EXPECT_FALSE(engine.IsSymbolKeyboardActive());

    // Open symbol keyboard with Ctrl + Alt + ,
    const uint16_t ctrl_alt = static_cast<uint16_t>(zzhuyin::mod::kControl | zzhuyin::mod::kAlt);
    auto o_open = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kOemComma, ctrl_alt));
    EXPECT_TRUE(o_open.handled);
    EXPECT_TRUE(o_open.symbol_keyboard_active);
    EXPECT_TRUE(engine.IsSymbolKeyboardActive());

    // Press '=' -> commits 「 and auto-closes symbol keyboard
    auto o1 = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kOemPlus));
    EXPECT_TRUE(o1.handled);
    EXPECT_EQ(o1.commit_text, u"「");
    EXPECT_FALSE(o1.symbol_keyboard_active);
    EXPECT_FALSE(engine.IsSymbolKeyboardActive());

    // Open again and verify Esc closes without committing
    engine.ProcessKey(MakeVkKey(zzhuyin::vk::kOemComma, ctrl_alt));
    EXPECT_TRUE(engine.IsSymbolKeyboardActive());
    auto o_esc = engine.ProcessKey(MakeVkKey(zzhuyin::vk::kEscape));
    EXPECT_TRUE(o_esc.handled);
    EXPECT_FALSE(o_esc.symbol_keyboard_active);
    EXPECT_FALSE(engine.IsSymbolKeyboardActive());

    // Normal Bopomofo resumes
    auto o_bo = engine.ProcessKey(MakeCharKey('1'));
    EXPECT_TRUE(o_bo.handled);
    EXPECT_EQ(o_bo.composition, u"ㄅ");
}
