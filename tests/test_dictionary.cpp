#include "test_runner.h"

#include "core/imechlex_dict.h"

#include <algorithm>
#include <filesystem>

namespace {

std::filesystem::path TargetPath(const char* rel) {
    return std::filesystem::path(ZZHUYIN_SOURCE_DIR) / "targets" / rel;
}

const zzhuyin::ImechlexDictionary& SharedDict() {
    static zzhuyin::ImechlexDictionary dict;
    static bool initialized = false;
    if (!initialized) {
        dict.LoadImechlex(TargetPath("DICTS/IMTCPH.IMD"));
        dict.LoadAssociateTables(TargetPath("lcptr.tbl"), TargetPath("lcphrase.tbl"));
        initialized = true;
    }
    return dict;
}

std::u16string Join(const std::vector<std::u16string>& items) {
    std::u16string out;
    for (const auto& s : items) {
        out += s;
    }
    return out;
}

} // namespace

TEST(T1_1_EncodingAndLookup) {
    const auto& dict = SharedDict();
    EXPECT_TRUE(dict.IsLoaded());
    EXPECT_EQ(dict.MinCid(), 6400u);
    EXPECT_EQ(dict.MaxCid(), 44602u);

    // ㄓ(15) ㄨ(2) ˋ(4) -> 0x7C10, bucket 7940
    const uint16_t code_zhu4 = zzhuyin::EncodeReading(15, 2, 0, 4);
    EXPECT_EQ(code_zhu4, 0x7C10);
    EXPECT_EQ(zzhuyin::ReadingBucket(code_zhu4), 7940);

    // ㄧ(1) ㄣ(10) 一聲(1) -> 0x0344, bucket 209
    const uint16_t code_yin1 = zzhuyin::EncodeReading(0, 1, 10, 1);
    EXPECT_EQ(code_yin1, 0x0344);
    EXPECT_EQ(zzhuyin::ReadingBucket(code_yin1), 209);

    // ㄓ(15) ㄨ(2) ㄥ(12) 一聲(1) -> 0x7D84, bucket 8033
    const uint16_t code_zhong1 = zzhuyin::EncodeReading(15, 2, 12, 1);
    EXPECT_EQ(code_zhong1, 0x7D84);
    EXPECT_EQ(zzhuyin::ReadingBucket(code_zhong1), 8033);

    zzhuyin::CharsetFilterConfig def{};
    auto r_zhu4   = dict.LookupReading(code_zhu4, def);
    auto r_yin1   = dict.LookupReading(code_yin1, def);
    auto r_zhong1 = dict.LookupReading(code_zhong1, def);

    EXPECT_TRUE(!r_zhu4.empty() && r_zhu4[0] == u"住");
    EXPECT_TRUE(!r_yin1.empty() && r_yin1[0] == u"因");
    EXPECT_TRUE(!r_zhong1.empty() && r_zhong1[0] == u"中");
}

TEST(T1_2_CharsetFilterDefault) {
    const auto& dict = SharedDict();
    zzhuyin::CharsetFilterConfig def{}; // Ext-A / CNS / HKSCS all false

    // ㄉ(5) ㄜ(3) ˙(5) -> 0x2874
    const uint16_t code_de5 = zzhuyin::EncodeReading(5, 0, 3, 5);
    EXPECT_EQ(code_de5, 0x2874);
    EXPECT_EQ(Join(dict.LookupReading(code_de5, def)), u"的得地");

    // ㄨ(2) ㄛ(2) ˇ(3) -> 0x044C
    const uint16_t code_wo3 = zzhuyin::EncodeReading(0, 2, 2, 3);
    EXPECT_EQ(code_wo3, 0x044C);
    EXPECT_EQ(Join(dict.LookupReading(code_wo3, def)), u"我婐捰");
}

TEST(T1_3_CharsetFilterExtA) {
    const auto& dict = SharedDict();
    zzhuyin::CharsetFilterConfig ext_a{};
    ext_a.include_ext_a = true;

    const uint16_t code_de5 = zzhuyin::EncodeReading(5, 0, 3, 5);
    auto res = dict.LookupReading(code_de5, ext_a);
    const bool has_ext_a_char =
        std::find(res.begin(), res.end(), std::u16string(1, u'\u4677')) != res.end(); // 䙷 (U+4677)
    EXPECT_TRUE(has_ext_a_char);
}

TEST(T1_4_AssociatePhraseLookup) {
    const auto& dict = SharedDict();
    EXPECT_TRUE(dict.HasAssociateTables());

    auto assoc_yi = dict.LookupAssociate(u'一');
    EXPECT_TRUE(assoc_yi.size() > 10);
    EXPECT_EQ(assoc_yi[0], u"般");
    EXPECT_EQ(assoc_yi[1], u"直");

    auto assoc_san = dict.LookupAssociate(u'三');
    EXPECT_TRUE(assoc_san.size() > 10);
    EXPECT_EQ(assoc_san[0], u"八");
    EXPECT_EQ(assoc_san[1], u"重");
}
