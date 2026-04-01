#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace helix {

inline uint32_t packFlashWord(const uint8_t existing[4],
                              std::size_t byteOffset,
                              const uint8_t* src,
                              std::size_t count) {
    uint8_t word[4];
    std::memcpy(word, existing, sizeof(word));
    for (std::size_t i = 0; i < count && (byteOffset + i) < sizeof(word); ++i) {
        word[byteOffset + i] = src[i];
    }

    uint32_t packed = 0u;
    std::memcpy(&packed, word, sizeof(packed));
    return packed;
}

} // namespace helix
