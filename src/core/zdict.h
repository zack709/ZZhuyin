#pragma once

#include "core/dictionary.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace zzhuyin {

// Standalone ZZhuyin dictionary backend (.zdict, magic "ZZDICT01").
// Uses the same O(1) 16384-bucket reading index and charset flag bits as IMECHLEX v2,
// while supporting open-source word tables built via tools/build_zdict.py.
class ZDictDictionary final : public IDictionary {
public:
    bool Load(const std::filesystem::path& zdict_path);
    bool IsLoaded() const { return loaded_; }

    size_t EntryCount() const { return entries_.size(); }
    size_t AssociateLeadCount() const { return assoc_ptrs_.size(); }

    std::vector<std::u16string> LookupReading(
        uint16_t reading_code, const CharsetFilterConfig& filter) const override;

    std::vector<std::u16string> LookupAssociate(char16_t lead_char) const override;

private:
    struct Entry {
        char16_t unicode;
        uint8_t  charset_flags;
    };

    struct AssocPtr {
        char16_t lead_wchar;
        uint32_t wchar_offset;
    };

    bool loaded_ = false;
    std::vector<uint32_t> bucket_offsets_; // [16385]
    std::vector<Entry>    entries_;
    std::vector<AssocPtr> assoc_ptrs_;
    std::vector<char16_t> assoc_pool_;
};

} // namespace zzhuyin
