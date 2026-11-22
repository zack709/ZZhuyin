#include "test_runner.h"

#include "core/engine.h"
#include "core/zdict.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

void AppendU16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void AppendU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

// Writes a minimal synthetic ZZDICT01 file for standalone unit testing.
std::filesystem::path WriteSyntheticZDict() {
    const auto path = std::filesystem::temp_directory_path() / "zzhuyin_test_synthetic.zdict";

    // Bucket for ㄉㄜ˙ (0x2874 >> 2 = 2589): 的(0x02), 得(0x02), 䙷(0x04 Ext-A)
    // Bucket for ㄅㄞˊ (0x08A8 >> 2 = 554): 白(0x02)
    constexpr uint16_t b_bai2 = zzhuyin::ReadingBucket(zzhuyin::EncodeReading(1, 0, 5, 2)); // 554
    constexpr uint16_t b_de5  = zzhuyin::ReadingBucket(zzhuyin::EncodeReading(5, 0, 3, 5)); // 2589

    std::vector<uint32_t> offsets(16385, 0);
    struct RawEntry { char16_t ch; uint8_t flags; };
    std::vector<RawEntry> entries;

    for (uint32_t b = 0; b < 16384; ++b) {
        offsets[b] = static_cast<uint32_t>(entries.size());
        if (b == b_bai2) {
            entries.push_back({u'白', 0x02});
        } else if (b == b_de5) {
            entries.push_back({u'的', 0x02});
            entries.push_back({u'得', 0x02});
            entries.push_back({u'\u4677', 0x04}); // 䙷 (Ext-A)
        }
    }
    offsets[16384] = static_cast<uint32_t>(entries.size());

    // Associate phrases for 白: 天, 雲
    std::vector<char16_t> pool = {u'天', u'\0', u'雲', u'\0'};

    std::vector<uint8_t> buf;
    const char magic[8] = {'Z','Z','D','I','C','T','0','1'};
    buf.insert(buf.end(), magic, magic + 8);
    AppendU32(buf, 1); // version
    AppendU32(buf, static_cast<uint32_t>(entries.size()));
    AppendU32(buf, 1); // assoc_lead_count
    AppendU32(buf, static_cast<uint32_t>(pool.size()));
    AppendU32(buf, 0); // flags
    AppendU32(buf, 0); // reserved

    for (uint32_t o : offsets) AppendU32(buf, o);
    for (const auto& e : entries) {
        AppendU16(buf, static_cast<uint16_t>(e.ch));
        buf.push_back(e.flags);
        buf.push_back(0);
    }
    // AssocPtr: {lead_wchar, 0, wchar_offset}
    AppendU16(buf, static_cast<uint16_t>(u'白'));
    AppendU16(buf, 0);
    AppendU32(buf, 0);
    for (char16_t ch : pool) AppendU16(buf, static_cast<uint16_t>(ch));

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    ofs.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return path;
}

} // namespace

TEST(ZDict_LoadLookupFilterAndAssociate) {
    const auto path = WriteSyntheticZDict();

    zzhuyin::ZDictDictionary dict;
    EXPECT_TRUE(dict.Load(path));
    EXPECT_EQ(dict.EntryCount(), 4u);
    EXPECT_EQ(dict.AssociateLeadCount(), 1u);

    const uint16_t code_de5 = zzhuyin::EncodeReading(5, 0, 3, 5);
    zzhuyin::CharsetFilterConfig def{};
    auto res_def = dict.LookupReading(code_de5, def);
    EXPECT_EQ(res_def.size(), 2u);
    EXPECT_EQ(res_def[0], u"的");
    EXPECT_EQ(res_def[1], u"得");

    zzhuyin::CharsetFilterConfig ext_a{};
    ext_a.include_ext_a = true;
    auto res_ext = dict.LookupReading(code_de5, ext_a);
    EXPECT_EQ(res_ext.size(), 3u);
    EXPECT_EQ(res_ext[2], u"\u4677");

    auto assoc = dict.LookupAssociate(u'白');
    EXPECT_EQ(assoc.size(), 2u);
    EXPECT_EQ(assoc[0], u"天");
    EXPECT_EQ(assoc[1], u"雲");

    // E2E check with ZhuyinEngine
    zzhuyin::EngineConfig cfg{};
    cfg.prompt_associate_phrase = true;
    zzhuyin::ZhuyinEngine engine(dict, cfg);

    auto feed = [&](char ch) {
        zzhuyin::KeyEvent ev{};
        ev.vk_code = static_cast<uint16_t>(ch);
        ev.unicode_char = static_cast<char16_t>(ch);
        return engine.ProcessKey(ev);
    };
    feed('1'); // ㄅ
    feed('9'); // ㄞ
    auto out = feed('6'); // ˊ -> single candidate 白 + associate 天/雲
    EXPECT_EQ(out.commit_text, u"白");
    EXPECT_TRUE(out.candidates.active);
    EXPECT_TRUE(out.candidates.is_associate);
    EXPECT_EQ(out.candidates.page_items[0], u"天");

    std::error_code ec;
    std::filesystem::remove(path, ec);
}
