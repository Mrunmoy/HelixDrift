#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "OtaManager.hpp"
#include "MockOtaFlashBackend.hpp"

using namespace helix;
using namespace helix::test;
using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static constexpr uint32_t kSlotSize = 384 * 1024; // 384 KB

// Compute CRC32 (IEEE 802.3) over a buffer, matching OtaManager's algorithm.
static uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFF;
}

// Fixture with a default-configured mock backend.
class OtaManagerTest : public ::testing::Test {
protected:
    MockOtaFlashBackend backend;
    OtaManager          mgr{backend};

    void SetUp() override {
        ON_CALL(backend, slotSize()).WillByDefault(Return(kSlotSize));
    }

    // Set up expectations for a successful begin().
    void expectSuccessfulBegin() {
        EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
        EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(true));
    }
};

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

TEST_F(OtaManagerTest, InitialStateIsIdle) {
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
    EXPECT_EQ(mgr.bytesReceived(), 0u);
}

// ---------------------------------------------------------------------------
// begin()
// ---------------------------------------------------------------------------

TEST_F(OtaManagerTest, BeginErasesSlotsAndTransitionsToReceiving) {
    expectSuccessfulBegin();

    const OtaStatus s = mgr.begin(1024, 0xDEADBEEF);

    EXPECT_EQ(s, OtaStatus::OK);
    EXPECT_EQ(mgr.state(), OtaState::RECEIVING);
    EXPECT_EQ(mgr.bytesReceived(), 0u);
}

TEST_F(OtaManagerTest, BeginFailsWhenImageTooLarge) {
    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
    EXPECT_CALL(backend, eraseSlot()).Times(0);

    const OtaStatus s = mgr.begin(kSlotSize + 1, 0);

    EXPECT_EQ(s, OtaStatus::ERROR_IMAGE_TOO_LARGE);
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
}

TEST_F(OtaManagerTest, BeginFailsWhenZeroImageSize) {
    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
    EXPECT_CALL(backend, eraseSlot()).Times(0);

    EXPECT_EQ(mgr.begin(0, 0), OtaStatus::ERROR_IMAGE_TOO_LARGE);
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
}

TEST_F(OtaManagerTest, BeginFailsWhenEraseFails) {
    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
    EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(false));

    EXPECT_EQ(mgr.begin(1024, 0), OtaStatus::ERROR_WRITE_FAILED);
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
}

TEST_F(OtaManagerTest, BeginWhileReceivingImplicitlyAbortsAndRestarts) {
    // OtaManager::begin() deliberately allows re-init from RECEIVING —
    // a new BEGIN is an implicit ABORT of any in-flight session. This
    // protects against stale state from a dropped BLE connection or a
    // failed upload (commit 4c53235, "fix PM vs DTS partition mismatch").
    // The second begin() therefore requires a second eraseSlot() call.
    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
    EXPECT_CALL(backend, eraseSlot()).Times(2).WillRepeatedly(Return(true));

    ASSERT_EQ(mgr.begin(1024, 0), OtaStatus::OK);
    ASSERT_EQ(mgr.state(), OtaState::RECEIVING);

    // Second begin() without an explicit abort() succeeds AND clears any
    // partial in-flight state (bytesReceived() resets to 0).
    EXPECT_EQ(mgr.begin(2048, 0), OtaStatus::OK);
    EXPECT_EQ(mgr.state(), OtaState::RECEIVING);
    EXPECT_EQ(mgr.bytesReceived(), 0u);
}

// ---------------------------------------------------------------------------
// writeChunk()
// ---------------------------------------------------------------------------

TEST_F(OtaManagerTest, WriteChunkFailsWhenIdle) {
    const uint8_t data[] = {1, 2, 3};
    EXPECT_EQ(mgr.writeChunk(0, data, 3), OtaStatus::ERROR_INVALID_STATE);
}

TEST_F(OtaManagerTest, WriteChunkCallsBackendAndAdvancesOffset) {
    expectSuccessfulBegin();
    ASSERT_EQ(mgr.begin(16, 0), OtaStatus::OK);

    const uint8_t chunk[] = {0xAA, 0xBB, 0xCC, 0xDD};
    EXPECT_CALL(backend, writeChunk(0, chunk, 4)).WillOnce(Return(true));

    EXPECT_EQ(mgr.writeChunk(0, chunk, 4), OtaStatus::OK);
    EXPECT_EQ(mgr.bytesReceived(), 4u);
}

TEST_F(OtaManagerTest, WriteChunkRejectsOutOfOrderOffset) {
    expectSuccessfulBegin();
    ASSERT_EQ(mgr.begin(16, 0), OtaStatus::OK);

    const uint8_t data[] = {1, 2, 3, 4};
    // Offset 4 before offset 0 has been written
    EXPECT_EQ(mgr.writeChunk(4, data, 4), OtaStatus::ERROR_BAD_OFFSET);
}

TEST_F(OtaManagerTest, WriteChunkFailsWhenBackendFails) {
    expectSuccessfulBegin();
    ASSERT_EQ(mgr.begin(16, 0), OtaStatus::OK);

    const uint8_t data[] = {1};
    EXPECT_CALL(backend, writeChunk(0, _, 1)).WillOnce(Return(false));

    EXPECT_EQ(mgr.writeChunk(0, data, 1), OtaStatus::ERROR_WRITE_FAILED);
    // Manager should have aborted back to IDLE
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
}

TEST_F(OtaManagerTest, WriteChunkFailsIfExceedsImageSize) {
    expectSuccessfulBegin();
    ASSERT_EQ(mgr.begin(4, 0), OtaStatus::OK);

    const uint8_t data[] = {1, 2, 3, 4, 5};
    // Writing 5 bytes into a 4-byte image
    EXPECT_EQ(mgr.writeChunk(0, data, 5), OtaStatus::ERROR_BAD_OFFSET);
}

// ---------------------------------------------------------------------------
// commit()
// ---------------------------------------------------------------------------

TEST_F(OtaManagerTest, CommitFailsWhenIdle) {
    EXPECT_EQ(mgr.commit(), OtaStatus::ERROR_INVALID_STATE);
}

TEST_F(OtaManagerTest, CommitFailsWhenIncomplete) {
    expectSuccessfulBegin();
    ASSERT_EQ(mgr.begin(8, 0), OtaStatus::OK);

    const uint8_t data[] = {1, 2, 3, 4};
    EXPECT_CALL(backend, writeChunk(0, _, 4)).WillOnce(Return(true));
    ASSERT_EQ(mgr.writeChunk(0, data, 4), OtaStatus::OK);

    // Only 4 of 8 bytes written
    EXPECT_EQ(mgr.commit(), OtaStatus::ERROR_INCOMPLETE);
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
}

TEST_F(OtaManagerTest, CommitFailsWithWrongCrc) {
    const uint8_t image[] = {0x01, 0x02, 0x03, 0x04};
    const uint32_t wrongCrc = 0xDEADBEEF;

    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
    EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(true));
    ASSERT_EQ(mgr.begin(sizeof(image), wrongCrc), OtaStatus::OK);

    EXPECT_CALL(backend, writeChunk(0, _, sizeof(image))).WillOnce(Return(true));
    ASSERT_EQ(mgr.writeChunk(0, image, sizeof(image)), OtaStatus::OK);

    EXPECT_EQ(mgr.commit(), OtaStatus::ERROR_INTEGRITY_FAILED);
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
}

TEST_F(OtaManagerTest, CommitSucceedsWithCorrectCrc) {
    const uint8_t image[] = {0x01, 0x02, 0x03, 0x04};
    const uint32_t goodCrc = crc32(image, sizeof(image));

    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
    EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(true));
    ASSERT_EQ(mgr.begin(sizeof(image), goodCrc), OtaStatus::OK);

    EXPECT_CALL(backend, writeChunk(0, _, sizeof(image))).WillOnce(Return(true));
    ASSERT_EQ(mgr.writeChunk(0, image, sizeof(image)), OtaStatus::OK);

    EXPECT_CALL(backend, setPendingUpgrade()).WillOnce(Return(true));
    EXPECT_EQ(mgr.commit(), OtaStatus::OK);
    EXPECT_EQ(mgr.state(), OtaState::COMMITTED);
}

TEST_F(OtaManagerTest, CommitFailsWhenSetPendingUpgradeFails) {
    const uint8_t image[] = {0xAB, 0xCD};
    const uint32_t goodCrc = crc32(image, sizeof(image));

    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
    EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(true));
    ASSERT_EQ(mgr.begin(sizeof(image), goodCrc), OtaStatus::OK);

    EXPECT_CALL(backend, writeChunk(0, _, sizeof(image))).WillOnce(Return(true));
    ASSERT_EQ(mgr.writeChunk(0, image, sizeof(image)), OtaStatus::OK);

    EXPECT_CALL(backend, setPendingUpgrade()).WillOnce(Return(false));
    EXPECT_EQ(mgr.commit(), OtaStatus::ERROR_UPGRADE_FAILED);
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
}

TEST_F(OtaManagerTest, CommitWorksAcrossMultipleChunks) {
    // Image sent in two sequential chunks.
    const uint8_t chunk1[] = {0x11, 0x22};
    const uint8_t chunk2[] = {0x33, 0x44};
    uint8_t fullImage[4];
    fullImage[0] = chunk1[0]; fullImage[1] = chunk1[1];
    fullImage[2] = chunk2[0]; fullImage[3] = chunk2[1];
    const uint32_t goodCrc = crc32(fullImage, 4);

    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
    EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(true));
    ASSERT_EQ(mgr.begin(4, goodCrc), OtaStatus::OK);

    EXPECT_CALL(backend, writeChunk(0, chunk1, 2)).WillOnce(Return(true));
    ASSERT_EQ(mgr.writeChunk(0, chunk1, 2), OtaStatus::OK);

    EXPECT_CALL(backend, writeChunk(2, chunk2, 2)).WillOnce(Return(true));
    ASSERT_EQ(mgr.writeChunk(2, chunk2, 2), OtaStatus::OK);

    EXPECT_CALL(backend, setPendingUpgrade()).WillOnce(Return(true));
    EXPECT_EQ(mgr.commit(), OtaStatus::OK);
    EXPECT_EQ(mgr.state(), OtaState::COMMITTED);
}

// ---------------------------------------------------------------------------
// abort()
// ---------------------------------------------------------------------------

TEST_F(OtaManagerTest, AbortFromIdleIsNoOp) {
    mgr.abort();
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
}

TEST_F(OtaManagerTest, AbortFromReceivingReturnsToIdle) {
    expectSuccessfulBegin();
    ASSERT_EQ(mgr.begin(1024, 0), OtaStatus::OK);
    EXPECT_EQ(mgr.state(), OtaState::RECEIVING);

    mgr.abort();
    EXPECT_EQ(mgr.state(), OtaState::IDLE);
    EXPECT_EQ(mgr.bytesReceived(), 0u);
}

// ---------------------------------------------------------------------------
// Re-use after terminal states
// ---------------------------------------------------------------------------

TEST_F(OtaManagerTest, CanBeginAgainAfterAbort) {
    expectSuccessfulBegin();
    ASSERT_EQ(mgr.begin(1024, 0), OtaStatus::OK);
    mgr.abort();

    expectSuccessfulBegin();
    EXPECT_EQ(mgr.begin(512, 0), OtaStatus::OK);
    EXPECT_EQ(mgr.state(), OtaState::RECEIVING);
}

TEST_F(OtaManagerTest, CanBeginAgainAfterCommit) {
    const uint8_t image[] = {0xDE, 0xAD};
    const uint32_t goodCrc = crc32(image, sizeof(image));

    EXPECT_CALL(backend, slotSize()).WillRepeatedly(Return(kSlotSize));
    EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(true));
    ASSERT_EQ(mgr.begin(sizeof(image), goodCrc), OtaStatus::OK);

    EXPECT_CALL(backend, writeChunk(0, _, sizeof(image))).WillOnce(Return(true));
    ASSERT_EQ(mgr.writeChunk(0, image, sizeof(image)), OtaStatus::OK);

    EXPECT_CALL(backend, setPendingUpgrade()).WillOnce(Return(true));
    ASSERT_EQ(mgr.commit(), OtaStatus::OK);

    // Second transfer after a commit should work
    EXPECT_CALL(backend, eraseSlot()).WillOnce(Return(true));
    EXPECT_EQ(mgr.begin(256, 0), OtaStatus::OK);
    EXPECT_EQ(mgr.state(), OtaState::RECEIVING);
}
