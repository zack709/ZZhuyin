#include "test_runner.h"

#include "core/candidate.h"

namespace {

std::vector<std::u16string> MakeItems(size_t n) {
    std::vector<std::u16string> items;
    items.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        items.emplace_back(1, static_cast<char16_t>(u'A' + (i % 26)));
    }
    return items;
}

} // namespace

TEST(T5_1_PaginationUnder9) {
    zzhuyin::CandidateManager cm;
    cm.OpenHomophone(MakeItems(5));
    EXPECT_EQ(cm.SelectedIndex(), 0u);
    cm.PageDown();
    EXPECT_EQ(cm.SelectedIndex(), 0u);
}

TEST(T5_2_PaginationWrap20) {
    zzhuyin::CandidateManager cm;
    cm.OpenHomophone(MakeItems(20));
    EXPECT_EQ(cm.SelectedIndex(), 0u);
    cm.PageDown();
    EXPECT_EQ(cm.SelectedIndex(), 9u);
    cm.PageDown();
    EXPECT_EQ(cm.SelectedIndex(), 18u);
    cm.PageDown();
    EXPECT_EQ(cm.SelectedIndex(), 0u);
}

TEST(T5_3_PenultimatePageLastColumnJump) {
    zzhuyin::CandidateManager cm;
    cm.OpenHomophone(MakeItems(20));
    // Move to idx 15 (page 2, col 6; last page only has 2 items: idx 18, 19)
    for (int i = 0; i < 15; ++i) {
        cm.MoveDown();
    }
    EXPECT_EQ(cm.SelectedIndex(), 15u);

    cm.PageDown();
    EXPECT_EQ(cm.SelectedIndex(), 19u); // N - 1

    cm.PageDown();
    EXPECT_EQ(cm.SelectedIndex(), 1u); // 19 % 9 == 1
}

TEST(T5_4_PageUpNoWrap) {
    zzhuyin::CandidateManager cm;
    cm.OpenHomophone(MakeItems(20));
    cm.MoveDown();
    cm.MoveDown();
    cm.MoveDown();
    EXPECT_EQ(cm.SelectedIndex(), 3u);
    cm.PageUp();
    EXPECT_EQ(cm.SelectedIndex(), 3u);
}

TEST(T5_5_UpDownWrap) {
    zzhuyin::CandidateManager cm;
    cm.OpenHomophone(MakeItems(20));
    EXPECT_EQ(cm.SelectedIndex(), 0u);
    cm.MoveUp();
    EXPECT_EQ(cm.SelectedIndex(), 19u);
    cm.MoveDown();
    EXPECT_EQ(cm.SelectedIndex(), 0u);
}
