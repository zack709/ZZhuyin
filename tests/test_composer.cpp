#include "test_runner.h"

#include "core/composer.h"
#include "core/key_mapper.h"

namespace {

void Feed(zzhuyin::BopomofoComposer& comp, zzhuyin::KeyboardLayout layout, char16_t ch) {
    auto mapped = zzhuyin::KeyMapper::MapBopomofo(layout, ch);
    EXPECT_TRUE(mapped.has_value());
    if (mapped) {
        comp.Input(mapped->slot, mapped->value);
    }
}

} // namespace

TEST(T2_1_SameSlotOverwrite) {
    zzhuyin::BopomofoComposer comp;
    Feed(comp, zzhuyin::KeyboardLayout::Standard, u'1'); // ㄅ
    EXPECT_EQ(comp.DisplayString(), u"ㄅ");
    Feed(comp, zzhuyin::KeyboardLayout::Standard, u'q'); // ㄆ
    EXPECT_EQ(comp.DisplayString(), u"ㄆ");
}

TEST(T2_2_OutOfOrderComposition) {
    zzhuyin::BopomofoComposer comp;
    Feed(comp, zzhuyin::KeyboardLayout::Standard, u'8'); // ㄚ (Final)
    Feed(comp, zzhuyin::KeyboardLayout::Standard, u'1'); // ㄅ (Initial)
    EXPECT_EQ(comp.DisplayString(), u"ㄅㄚ");
}

TEST(T2_3_MedialOverwrite) {
    zzhuyin::BopomofoComposer comp;
    Feed(comp, zzhuyin::KeyboardLayout::Standard, u'q'); // ㄆ
    Feed(comp, zzhuyin::KeyboardLayout::Standard, u'u'); // ㄧ
    Feed(comp, zzhuyin::KeyboardLayout::Standard, u'8'); // ㄚ
    Feed(comp, zzhuyin::KeyboardLayout::Standard, u'j'); // ㄨ (overwrites ㄧ)
    EXPECT_EQ(comp.DisplayString(), u"ㄆㄨㄚ");
}
