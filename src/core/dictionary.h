#pragma once

#include "core/types.h"

#include <string>
#include <vector>

namespace zzhuyin {

class IDictionary {
public:
    virtual ~IDictionary() = default;

    // Look up candidate characters for a 16-bit Bopomofo reading code.
    virtual std::vector<std::u16string> LookupReading(
        uint16_t reading_code, const CharsetFilterConfig& filter) const = 0;

    // Look up associate phrases (相關字詞) following a committed BMP character.
    virtual std::vector<std::u16string> LookupAssociate(char16_t lead_char) const = 0;
};

} // namespace zzhuyin
