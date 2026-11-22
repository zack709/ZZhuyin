#pragma once

#include "core/dictionary.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace zzhuyin {

class ImechlexDictionary final : public IDictionary {
public:
    bool LoadImechlex(const std::filesystem::path& imd_path);
    bool LoadAssociateTables(
        const std::filesystem::path& lcptr_path,
        const std::filesystem::path& lcphrase_path);

    bool IsLoaded() const { return loaded_; }
    bool HasAssociateTables() const { return !assoc_ptrs_.empty() && !assoc_phrases_.empty(); }

    uint32_t MinCid() const { return min_cid_; }
    uint32_t MaxCid() const { return max_cid_; }
    uint32_t EntryCount() const { return max_cid_ - min_cid_; }

    std::vector<std::u16string> LookupReading(
        uint16_t reading_code, const CharsetFilterConfig& filter) const override;

    std::vector<std::u16string> LookupAssociate(char16_t lead_char) const override;

    static bool PassCharsetFilter(uint8_t flags, const CharsetFilterConfig& filter);

private:
    struct AssociatePtrEntry {
        char16_t lead_wchar;
        uint32_t wchar_offset;
    };

    bool loaded_ = false;
    uint32_t min_cid_ = 0;
    uint32_t max_cid_ = 0;
    uint32_t flags_ = 0;

    std::vector<uint8_t>  sec1_attr_;      // [N]
    std::vector<char16_t> sec2_unicode_;   // [N]
    std::vector<uint16_t> sec3_reading_;   // [N]
    std::vector<uint16_t> bucket_offsets_; // [16385]
    std::vector<uint16_t> cand_cids_;

    std::vector<AssociatePtrEntry> assoc_ptrs_;
    std::vector<char16_t>          assoc_phrases_;
};

} // namespace zzhuyin
