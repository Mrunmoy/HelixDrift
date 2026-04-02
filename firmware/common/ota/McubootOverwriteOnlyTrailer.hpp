#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace helix {

struct McubootOverwriteOnlyTrailer {
    static constexpr uint32_t kAlign = 4u;
    static constexpr uint32_t kMagicSize = 16u;
    static constexpr uint32_t kImageOkSize = kAlign;
    static constexpr uint32_t kCopyDoneSize = kAlign;
    static constexpr uint32_t kSwapInfoSize = kAlign;
    static constexpr uint32_t kReservedSize =
        kMagicSize + kImageOkSize + kCopyDoneSize + kSwapInfoSize;

    static constexpr uint8_t kBootFlagSet = 1u;
    static constexpr uint8_t kBootSwapTypeTest = 2u;
    static constexpr uint8_t kBootSwapTypePerm = 3u;

    static constexpr std::array<uint8_t, kMagicSize> kMagic = {
        0x04u, 0x00u,
        0x2du, 0xe1u, 0x5du, 0x29u, 0x41u, 0x0bu,
        0x8du, 0x77u, 0x67u, 0x9cu, 0x11u, 0x0fu, 0x1fu, 0x8au
    };

    static constexpr uint32_t maxImageSize(uint32_t slotSize) {
        return (slotSize > kReservedSize) ? (slotSize - kReservedSize) : 0u;
    }

    static constexpr bool canStoreImage(uint32_t slotSize, uint32_t imageSize) {
        return imageSize <= maxImageSize(slotSize);
    }

    static constexpr uint32_t magicOffset(uint32_t slotSize) {
        return slotSize - kMagicSize;
    }

    static constexpr uint32_t imageOkOffset(uint32_t slotSize) {
        return magicOffset(slotSize) - kAlign;
    }

    static constexpr uint32_t copyDoneOffset(uint32_t slotSize) {
        return imageOkOffset(slotSize) - kAlign;
    }

    static constexpr uint32_t swapInfoOffset(uint32_t slotSize) {
        return copyDoneOffset(slotSize) - kAlign;
    }

    static constexpr uint8_t makeSwapInfo(uint8_t imageNum, bool permanent) {
        return static_cast<uint8_t>((imageNum << 4) |
               (permanent ? kBootSwapTypePerm : kBootSwapTypeTest));
    }

    static bool markPending(uint8_t* slot, uint32_t slotSize, bool permanent, uint8_t imageNum = 0u) {
        if (slot == nullptr || !canStoreImage(slotSize, 0u) || imageNum > 0x0Fu) {
            return false;
        }

        slot[swapInfoOffset(slotSize)] = makeSwapInfo(imageNum, permanent);
        if (permanent) {
            slot[imageOkOffset(slotSize)] = kBootFlagSet;
        }
        for (std::size_t i = 0; i < kMagic.size(); ++i) {
            slot[magicOffset(slotSize) + i] = kMagic[i];
        }
        return true;
    }
};

} // namespace helix
