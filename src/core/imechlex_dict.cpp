#include "core/imechlex_dict.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace zzhuyin {

namespace {

bool ReadAllBytes(const std::filesystem::path& path, std::vector<uint8_t>& out) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) {
        return false;
    }
    const std::streamsize size = ifs.tellg();
    if (size < 0) {
        return false;
    }
    out.resize(static_cast<size_t>(size));
    ifs.seekg(0, std::ios::beg);
    if (size > 0 && !ifs.read(reinterpret_cast<char*>(out.data()), size)) {
        out.clear();
        return false;
    }
    return true;
}

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

bool ImechlexDictionary::LoadImechlex(const std::filesystem::path& imd_path) {
    loaded_ = false;
    std::vector<uint8_t> data;
    if (!ReadAllBytes(imd_path, data) || data.size() < 0x38) {
        return false;
    }

    if (std::memcmp(data.data(), "IMECHLEX", 8) != 0) {
        return false;
    }

    const uint32_t minor_ver  = ReadU32LE(data.data() + 0x10);
    const uint32_t major_ver  = ReadU32LE(data.data() + 0x14);
    const uint32_t sec1_off   = ReadU32LE(data.data() + 0x18);
    const uint32_t sec2_off   = ReadU32LE(data.data() + 0x1C);
    const uint32_t sec3_off   = ReadU32LE(data.data() + 0x20);
    const uint32_t sec4_off   = ReadU32LE(data.data() + 0x24);
    const uint32_t total_size = ReadU32LE(data.data() + 0x28);
    const uint32_t max_cid    = ReadU32LE(data.data() + 0x2C);
    const uint32_t min_cid    = ReadU32LE(data.data() + 0x30);
    const uint32_t flags      = ReadU32LE(data.data() + 0x34);

    if (minor_ver != 0 || major_ver != 2 || total_size > data.size() || max_cid <= min_cid) {
        return false;
    }

    const size_t count = static_cast<size_t>(max_cid - min_cid);
    constexpr size_t kBucketCount = 16385;
    constexpr size_t kBucketTableBytes = kBucketCount * 2; // 0x8002

    if (sec1_off + count > total_size ||
        sec2_off + count * 2 > total_size ||
        sec3_off + count * 2 > total_size ||
        sec4_off + kBucketTableBytes > total_size) {
        return false;
    }

    min_cid_ = min_cid;
    max_cid_ = max_cid;
    flags_   = flags;

    sec1_attr_.assign(data.begin() + sec1_off, data.begin() + sec1_off + count);

    sec2_unicode_.resize(count);
    for (size_t i = 0; i < count; ++i) {
        sec2_unicode_[i] = static_cast<char16_t>(ReadU16LE(data.data() + sec2_off + i * 2));
    }

    sec3_reading_.resize(count);
    for (size_t i = 0; i < count; ++i) {
        sec3_reading_[i] = ReadU16LE(data.data() + sec3_off + i * 2);
    }

    bucket_offsets_.resize(kBucketCount);
    for (size_t i = 0; i < kBucketCount; ++i) {
        bucket_offsets_[i] = ReadU16LE(data.data() + sec4_off + i * 2);
    }

    const size_t cands_bytes = total_size - (sec4_off + kBucketTableBytes);
    const size_t cands_count = cands_bytes / 2;
    cand_cids_.resize(cands_count);
    for (size_t i = 0; i < cands_count; ++i) {
        cand_cids_[i] = ReadU16LE(data.data() + sec4_off + kBucketTableBytes + i * 2);
    }

    loaded_ = true;
    return true;
}

bool ImechlexDictionary::LoadAssociateTables(
    const std::filesystem::path& lcptr_path,
    const std::filesystem::path& lcphrase_path) {
    assoc_ptrs_.clear();
    assoc_phrases_.clear();

    std::vector<uint8_t> ptr_bytes;
    std::vector<uint8_t> phrase_bytes;
    if (!ReadAllBytes(lcptr_path, ptr_bytes) || !ReadAllBytes(lcphrase_path, phrase_bytes)) {
        return false;
    }
    if (ptr_bytes.size() < 6 || (ptr_bytes.size() - 6) % 6 != 0 || phrase_bytes.size() % 2 != 0) {
        return false;
    }

    const size_t entry_count = (ptr_bytes.size() - 6) / 6;
    assoc_ptrs_.resize(entry_count);
    for (size_t i = 0; i < entry_count; ++i) {
        const uint8_t* p = ptr_bytes.data() + 6 + i * 6;
        assoc_ptrs_[i].lead_wchar   = static_cast<char16_t>(ReadU16LE(p));
        assoc_ptrs_[i].wchar_offset = ReadU32LE(p + 2);
    }

    const size_t phrase_wchars = phrase_bytes.size() / 2;
    assoc_phrases_.resize(phrase_wchars);
    for (size_t i = 0; i < phrase_wchars; ++i) {
        assoc_phrases_[i] = static_cast<char16_t>(ReadU16LE(phrase_bytes.data() + i * 2));
    }

    return true;
}

bool ImechlexDictionary::PassCharsetFilter(uint8_t flags, const CharsetFilterConfig& filter) {
    if (filter.output_big5_only && (flags & 0x02) == 0) {
        return false;
    }
    if ((flags & 0x0D) != 0) {
        const bool allowed = (filter.include_ext_a && (flags & 0x04))
                          || (filter.allow_cns     && (flags & 0x01))
                          || (filter.include_hkscs && (flags & 0x08));
        if (!allowed) {
            return false;
        }
    }
    return true;
}

std::vector<std::u16string> ImechlexDictionary::LookupReading(
    uint16_t reading_code, const CharsetFilterConfig& filter) const {
    std::vector<std::u16string> result;
    if (!loaded_) {
        return result;
    }

    const uint16_t bucket = ReadingBucket(reading_code);
    if (bucket >= 16384) {
        return result;
    }

    const size_t start = bucket_offsets_[bucket];
    const size_t end   = bucket_offsets_[bucket + 1];
    if (start >= end || end > cand_cids_.size()) {
        return result;
    }

    result.reserve(end - start);
    for (size_t i = start; i < end; ++i) {
        const uint32_t cid = cand_cids_[i];
        if (cid < min_cid_ || cid >= max_cid_) {
            continue;
        }
        const size_t idx = static_cast<size_t>(cid - min_cid_);
        if (!PassCharsetFilter(sec1_attr_[idx], filter)) {
            continue;
        }
        result.emplace_back(1, sec2_unicode_[idx]);
    }
    return result;
}

std::vector<std::u16string> ImechlexDictionary::LookupAssociate(char16_t lead_char) const {
    std::vector<std::u16string> result;
    if (assoc_ptrs_.empty() || assoc_phrases_.empty()) {
        return result;
    }

    auto it = std::lower_bound(
        assoc_ptrs_.begin(),
        assoc_ptrs_.end(),
        lead_char,
        [](const AssociatePtrEntry& entry, char16_t key) {
            return entry.lead_wchar < key;
        });

    if (it == assoc_ptrs_.end() || it->lead_wchar != lead_char) {
        return result;
    }

    const size_t start_off = it->wchar_offset;
    const size_t next_idx  = static_cast<size_t>(it - assoc_ptrs_.begin()) + 1;
    const size_t end_off   = (next_idx < assoc_ptrs_.size())
                           ? static_cast<size_t>(assoc_ptrs_[next_idx].wchar_offset)
                           : assoc_phrases_.size();

    if (start_off >= assoc_phrases_.size() || end_off > assoc_phrases_.size() || start_off >= end_off) {
        return result;
    }

    size_t pos = start_off;
    while (pos < end_off) {
        size_t nul = pos;
        while (nul < end_off && assoc_phrases_[nul] != u'\0') {
            ++nul;
        }
        if (nul > pos) {
            result.emplace_back(assoc_phrases_.begin() + pos, assoc_phrases_.begin() + nul);
        }
        pos = nul + 1;
    }

    return result;
}

} // namespace zzhuyin
