#include "core/zdict.h"

#include "core/imechlex_dict.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace zzhuyin {

namespace {

uint16_t ReadU16LE(const uint8_t* ptr) {
    return static_cast<uint16_t>(ptr[0]) | (static_cast<uint16_t>(ptr[1]) << 8);
}

uint32_t ReadU32LE(const uint8_t* ptr) {
    return static_cast<uint32_t>(ptr[0])
         | (static_cast<uint32_t>(ptr[1]) << 8)
         | (static_cast<uint32_t>(ptr[2]) << 16)
         | (static_cast<uint32_t>(ptr[3]) << 24);
}

} // namespace

bool ZDictDictionary::Load(const std::filesystem::path& zdict_path) {
    loaded_ = false;
    bucket_offsets_.clear();
    entries_.clear();
    assoc_ptrs_.clear();
    assoc_pool_.clear();

    std::ifstream ifs(zdict_path, std::ios::binary | std::ios::ate);
    if (!ifs) return false;

    const std::streamsize size = ifs.tellg();
    if (size < 32) return false;

    std::vector<uint8_t> data(static_cast<size_t>(size));
    ifs.seekg(0, std::ios::beg);
    if (!ifs.read(reinterpret_cast<char*>(data.data()), size)) {
        return false;
    }

    if (std::memcmp(data.data(), "ZZDICT01", 8) != 0) {
        return false;
    }

    const uint32_t version           = ReadU32LE(data.data() + 0x08);
    const uint32_t entry_count       = ReadU32LE(data.data() + 0x0C);
    const uint32_t assoc_lead_count  = ReadU32LE(data.data() + 0x10);
    const uint32_t assoc_pool_wchars = ReadU32LE(data.data() + 0x14);
    if (version != 1) {
        return false;
    }

    constexpr size_t kHeaderBytes = 32;
    constexpr size_t kBucketCount = 16385;
    constexpr size_t kBucketBytes = kBucketCount * 4;

    const size_t entries_bytes    = static_cast<size_t>(entry_count) * 4;
    const size_t assoc_ptr_bytes  = static_cast<size_t>(assoc_lead_count) * 8;
    const size_t assoc_pool_bytes = static_cast<size_t>(assoc_pool_wchars) * 2;
    const size_t expected_size =
        kHeaderBytes + kBucketBytes + entries_bytes + assoc_ptr_bytes + assoc_pool_bytes;

    if (data.size() < expected_size) {
        return false;
    }

    size_t off = kHeaderBytes;

    bucket_offsets_.resize(kBucketCount);
    for (size_t i = 0; i < kBucketCount; ++i) {
        bucket_offsets_[i] = ReadU32LE(data.data() + off + i * 4);
    }
    off += kBucketBytes;

    entries_.resize(entry_count);
    for (size_t i = 0; i < entry_count; ++i) {
        const uint8_t* p = data.data() + off + i * 4;
        entries_[i].unicode       = static_cast<char16_t>(ReadU16LE(p));
        entries_[i].charset_flags = p[2];
    }
    off += entries_bytes;

    assoc_ptrs_.resize(assoc_lead_count);
    for (size_t i = 0; i < assoc_lead_count; ++i) {
        const uint8_t* p = data.data() + off + i * 8;
        assoc_ptrs_[i].lead_wchar   = static_cast<char16_t>(ReadU16LE(p));
        assoc_ptrs_[i].wchar_offset = ReadU32LE(p + 4);
    }
    off += assoc_ptr_bytes;

    assoc_pool_.resize(assoc_pool_wchars);
    for (size_t i = 0; i < assoc_pool_wchars; ++i) {
        assoc_pool_[i] = static_cast<char16_t>(ReadU16LE(data.data() + off + i * 2));
    }

    loaded_ = true;
    return true;
}

std::vector<std::u16string> ZDictDictionary::LookupReading(
    uint16_t reading_code, const CharsetFilterConfig& filter) const {
    std::vector<std::u16string> result;
    if (!loaded_) return result;

    const uint16_t bucket = ReadingBucket(reading_code);
    if (bucket >= 16384) return result;

    const size_t start = bucket_offsets_[bucket];
    const size_t end   = bucket_offsets_[bucket + 1];
    if (start >= end || end > entries_.size()) return result;

    result.reserve(end - start);
    for (size_t i = start; i < end; ++i) {
        if (!ImechlexDictionary::PassCharsetFilter(entries_[i].charset_flags, filter)) {
            continue;
        }
        result.emplace_back(1, entries_[i].unicode);
    }
    return result;
}

std::vector<std::u16string> ZDictDictionary::LookupAssociate(char16_t lead_char) const {
    std::vector<std::u16string> result;
    if (!loaded_ || assoc_ptrs_.empty() || assoc_pool_.empty()) {
        return result;
    }

    auto it = std::lower_bound(
        assoc_ptrs_.begin(),
        assoc_ptrs_.end(),
        lead_char,
        [](const AssocPtr& entry, char16_t key) {
            return entry.lead_wchar < key;
        });

    if (it == assoc_ptrs_.end() || it->lead_wchar != lead_char) {
        return result;
    }

    const size_t start_off = it->wchar_offset;
    const size_t next_idx  = static_cast<size_t>(it - assoc_ptrs_.begin()) + 1;
    const size_t end_off   = (next_idx < assoc_ptrs_.size())
                           ? static_cast<size_t>(assoc_ptrs_[next_idx].wchar_offset)
                           : assoc_pool_.size();

    if (start_off >= assoc_pool_.size() || end_off > assoc_pool_.size() || start_off >= end_off) {
        return result;
    }

    size_t pos = start_off;
    while (pos < end_off) {
        size_t nul = pos;
        while (nul < end_off && assoc_pool_[nul] != u'\0') {
            ++nul;
        }
        if (nul > pos) {
            result.emplace_back(assoc_pool_.begin() + pos, assoc_pool_.begin() + nul);
        }
        pos = nul + 1;
    }

    return result;
}

} // namespace zzhuyin
