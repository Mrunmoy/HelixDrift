#include <gtest/gtest.h>

#include "FlashWordPacker.hpp"

using namespace helix;

TEST(FlashWordPackerTest, PacksLeadingPartialIntoExistingWord) {
    const uint8_t existing[4] = {0x11, 0x22, 0x33, 0x44};
    const uint8_t src[2] = {0xAA, 0xBB};

    const uint32_t packed = packFlashWord(existing, 1, src, 2);
    EXPECT_EQ(packed, 0x44BB'AA11u);
}

TEST(FlashWordPackerTest, PreservesExistingBytesOutsideUpdatedRange) {
    const uint8_t existing[4] = {0x10, 0x20, 0x30, 0x40};
    const uint8_t src[1] = {0x99};

    const uint32_t packed = packFlashWord(existing, 3, src, 1);
    EXPECT_EQ(packed, 0x99'30'20'10u);
}

TEST(FlashWordPackerTest, TruncatesWriteAtWordBoundary) {
    const uint8_t existing[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    const uint8_t src[4] = {0x01, 0x02, 0x03, 0x04};

    const uint32_t packed = packFlashWord(existing, 2, src, 4);
    EXPECT_EQ(packed, 0x02'01'FF'FFu);
}
