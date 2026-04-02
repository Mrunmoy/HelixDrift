#include <gtest/gtest.h>

#include "McubootOverwriteOnlyTrailer.hpp"

#include <vector>

using helix::McubootOverwriteOnlyTrailer;

TEST(McubootOverwriteOnlyTrailerTest, ReportsReservedAndMaxImageSize) {
    EXPECT_EQ(McubootOverwriteOnlyTrailer::kReservedSize, 28u);
    EXPECT_EQ(McubootOverwriteOnlyTrailer::maxImageSize(144u * 1024u), 147428u);
    EXPECT_TRUE(McubootOverwriteOnlyTrailer::canStoreImage(4096u, 4068u));
    EXPECT_FALSE(McubootOverwriteOnlyTrailer::canStoreImage(4096u, 4069u));
}

TEST(McubootOverwriteOnlyTrailerTest, MarksPermanentPendingTrailer) {
    constexpr uint32_t kSlotSize = 4096u;
    std::vector<uint8_t> slot(kSlotSize, 0xFFu);

    ASSERT_TRUE(McubootOverwriteOnlyTrailer::markPending(slot.data(), kSlotSize, true, 0u));

    EXPECT_EQ(slot[McubootOverwriteOnlyTrailer::swapInfoOffset(kSlotSize)], 0x03u);
    EXPECT_EQ(slot[McubootOverwriteOnlyTrailer::imageOkOffset(kSlotSize)], 0x01u);
    EXPECT_EQ(slot[McubootOverwriteOnlyTrailer::copyDoneOffset(kSlotSize)], 0xFFu);
    for (std::size_t i = 0; i < McubootOverwriteOnlyTrailer::kMagic.size(); ++i) {
        EXPECT_EQ(slot[McubootOverwriteOnlyTrailer::magicOffset(kSlotSize) + i],
                  McubootOverwriteOnlyTrailer::kMagic[i]);
    }
}

TEST(McubootOverwriteOnlyTrailerTest, MarksTestPendingWithoutImageOk) {
    constexpr uint32_t kSlotSize = 4096u;
    std::vector<uint8_t> slot(kSlotSize, 0xFFu);

    ASSERT_TRUE(McubootOverwriteOnlyTrailer::markPending(slot.data(), kSlotSize, false, 2u));

    EXPECT_EQ(slot[McubootOverwriteOnlyTrailer::swapInfoOffset(kSlotSize)], 0x22u);
    EXPECT_EQ(slot[McubootOverwriteOnlyTrailer::imageOkOffset(kSlotSize)], 0xFFu);
}
