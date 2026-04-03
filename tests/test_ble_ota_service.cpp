#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "BleOtaService.hpp"
#include "OtaManager.hpp"
#include "OtaTargetIdentity.hpp"

using namespace helix;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

// ---------------------------------------------------------------------------
// Mock OtaManager — wrap via a thin interface so BleOtaService is testable
// without a real flash backend.
// ---------------------------------------------------------------------------

class MockOtaManager : public IOtaManager {
public:
    MOCK_METHOD(OtaStatus, begin,    (uint32_t imageSize, uint32_t expectedCrc32), (override));
    MOCK_METHOD(OtaStatus, writeChunk,(uint32_t offset, const uint8_t* data, size_t len), (override));
    MOCK_METHOD(OtaStatus, commit,   (), (override));
    MOCK_METHOD(void,      abort,    (), (override));
    MOCK_METHOD(OtaState,  state,    (), (const, override));
    MOCK_METHOD(uint32_t,  bytesReceived, (), (const, override));
};

// ---------------------------------------------------------------------------
// Helpers to build control packets
// ---------------------------------------------------------------------------

static std::vector<uint8_t> makeBeginPacket(uint32_t size, uint32_t crc,
                                            uint32_t targetId = kOtaTargetIdNrf52dkNrf52832) {
    return {
        0x01,
        static_cast<uint8_t>(size       & 0xFF),
        static_cast<uint8_t>(size >> 8  & 0xFF),
        static_cast<uint8_t>(size >> 16 & 0xFF),
        static_cast<uint8_t>(size >> 24 & 0xFF),
        static_cast<uint8_t>(crc        & 0xFF),
        static_cast<uint8_t>(crc  >> 8  & 0xFF),
        static_cast<uint8_t>(crc  >> 16 & 0xFF),
        static_cast<uint8_t>(crc  >> 24 & 0xFF),
        static_cast<uint8_t>(targetId       & 0xFF),
        static_cast<uint8_t>(targetId >> 8  & 0xFF),
        static_cast<uint8_t>(targetId >> 16 & 0xFF),
        static_cast<uint8_t>(targetId >> 24 & 0xFF),
    };
}

static std::vector<uint8_t> makeDataPacket(uint32_t offset,
                                            const uint8_t* data,
                                            size_t         len) {
    std::vector<uint8_t> pkt = {
        static_cast<uint8_t>(offset       & 0xFF),
        static_cast<uint8_t>(offset >> 8  & 0xFF),
        static_cast<uint8_t>(offset >> 16 & 0xFF),
        static_cast<uint8_t>(offset >> 24 & 0xFF),
    };
    pkt.insert(pkt.end(), data, data + len);
    return pkt;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class BleOtaServiceTest : public ::testing::Test {
protected:
    StrictMock<MockOtaManager> mgr;
    BleOtaService              svc{mgr, kOtaTargetIdNrf52dkNrf52832};
};

// ---------------------------------------------------------------------------
// Control characteristic — begin command (0x01)
// ---------------------------------------------------------------------------

TEST_F(BleOtaServiceTest, ControlBeginCallsManagerBeginWithCorrectArgs) {
    auto pkt = makeBeginPacket(0x1234, 0xDEADBEEF);
    EXPECT_CALL(mgr, begin(0x1234u, 0xDEADBEEFu)).WillOnce(Return(OtaStatus::OK));

    EXPECT_EQ(OtaStatus::OK, svc.handleControlWrite(pkt.data(), pkt.size()));
}

TEST_F(BleOtaServiceTest, ControlBeginPropagatesManagerError) {
    auto pkt = makeBeginPacket(0x1234, 0xABCD);
    EXPECT_CALL(mgr, begin(0x1234u, 0xABCDu))
        .WillOnce(Return(OtaStatus::ERROR_IMAGE_TOO_LARGE));

    EXPECT_EQ(OtaStatus::ERROR_IMAGE_TOO_LARGE,
              svc.handleControlWrite(pkt.data(), pkt.size()));
}

TEST_F(BleOtaServiceTest, ControlBeginReturnsBadOffsetForShortPacket) {
    // 12 bytes instead of 13 (missing last target-id byte).
    const uint8_t shortPkt[] = {0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x01, 0x20, 0x83};
    EXPECT_EQ(OtaStatus::ERROR_BAD_OFFSET,
              svc.handleControlWrite(shortPkt, sizeof(shortPkt)));
}

TEST_F(BleOtaServiceTest, ControlBeginRejectsWrongTargetId) {
    auto pkt = makeBeginPacket(0x1234, 0xDEADBEEF, kOtaTargetIdNrf52840Dongle);
    EXPECT_EQ(OtaStatus::ERROR_WRONG_TARGET,
              svc.handleControlWrite(pkt.data(), pkt.size()));
}

// ---------------------------------------------------------------------------
// Control characteristic — abort command (0x02)
// ---------------------------------------------------------------------------

TEST_F(BleOtaServiceTest, ControlAbortCallsManagerAbort) {
    const uint8_t pkt[] = {0x02};
    EXPECT_CALL(mgr, abort());

    EXPECT_EQ(OtaStatus::OK, svc.handleControlWrite(pkt, sizeof(pkt)));
}

// ---------------------------------------------------------------------------
// Control characteristic — commit command (0x03)
// ---------------------------------------------------------------------------

TEST_F(BleOtaServiceTest, ControlCommitCallsManagerCommit) {
    const uint8_t pkt[] = {0x03};
    EXPECT_CALL(mgr, commit()).WillOnce(Return(OtaStatus::OK));

    EXPECT_EQ(OtaStatus::OK, svc.handleControlWrite(pkt, sizeof(pkt)));
}

TEST_F(BleOtaServiceTest, ControlCommitPropagatesManagerError) {
    const uint8_t pkt[] = {0x03};
    EXPECT_CALL(mgr, commit()).WillOnce(Return(OtaStatus::ERROR_INTEGRITY_FAILED));

    EXPECT_EQ(OtaStatus::ERROR_INTEGRITY_FAILED,
              svc.handleControlWrite(pkt, sizeof(pkt)));
}

// ---------------------------------------------------------------------------
// Control characteristic — unknown command
// ---------------------------------------------------------------------------

TEST_F(BleOtaServiceTest, ControlUnknownCommandReturnsInvalidState) {
    const uint8_t pkt[] = {0xFF};
    EXPECT_EQ(OtaStatus::ERROR_INVALID_STATE,
              svc.handleControlWrite(pkt, sizeof(pkt)));
}

TEST_F(BleOtaServiceTest, ControlEmptyPacketReturnsInvalidState) {
    EXPECT_EQ(OtaStatus::ERROR_INVALID_STATE,
              svc.handleControlWrite(nullptr, 0));
}

// ---------------------------------------------------------------------------
// Data characteristic — chunk write
// ---------------------------------------------------------------------------

TEST_F(BleOtaServiceTest, DataWriteCallsWriteChunkWithOffsetAndPayload) {
    const uint8_t payload[] = {0xAA, 0xBB, 0xCC};
    auto pkt = makeDataPacket(128u, payload, sizeof(payload));

    EXPECT_CALL(mgr, writeChunk(128u, _, sizeof(payload)))
        .WillOnce(Return(OtaStatus::OK));

    EXPECT_EQ(OtaStatus::OK, svc.handleDataWrite(pkt.data(), pkt.size()));
}

TEST_F(BleOtaServiceTest, DataWritePropagatesManagerError) {
    const uint8_t payload[] = {0x01};
    auto pkt = makeDataPacket(0u, payload, sizeof(payload));

    EXPECT_CALL(mgr, writeChunk(0u, _, sizeof(payload)))
        .WillOnce(Return(OtaStatus::ERROR_WRITE_FAILED));

    EXPECT_EQ(OtaStatus::ERROR_WRITE_FAILED,
              svc.handleDataWrite(pkt.data(), pkt.size()));
}

TEST_F(BleOtaServiceTest, DataWriteReturnsInvalidStateForShortPacket) {
    // Only 3 bytes — not enough for the 4-byte offset header.
    const uint8_t short_pkt[] = {0x00, 0x00, 0x00};
    EXPECT_EQ(OtaStatus::ERROR_INVALID_STATE,
              svc.handleDataWrite(short_pkt, sizeof(short_pkt)));
}

TEST_F(BleOtaServiceTest, DataWriteReturnsInvalidStateForNullData) {
    EXPECT_EQ(OtaStatus::ERROR_INVALID_STATE,
              svc.handleDataWrite(nullptr, 0));
}

TEST_F(BleOtaServiceTest, DataWriteOffsetHeaderOnlyNoPayloadIsValid) {
    // 4-byte offset with zero-length payload: valid (no-op write).
    const uint8_t pkt[] = {0x00, 0x00, 0x00, 0x00};
    EXPECT_CALL(mgr, writeChunk(0u, _, 0u)).WillOnce(Return(OtaStatus::OK));

    EXPECT_EQ(OtaStatus::OK, svc.handleDataWrite(pkt, sizeof(pkt)));
}

// ---------------------------------------------------------------------------
// Status read
// ---------------------------------------------------------------------------

TEST_F(BleOtaServiceTest, GetStatusReturnsStateBytesReceivedAndLastStatus) {
    EXPECT_CALL(mgr, state()).WillOnce(Return(OtaState::RECEIVING));
    EXPECT_CALL(mgr, bytesReceived()).WillOnce(Return(512u));

    uint8_t buf[BleOtaService::kStatusLen] = {};
    size_t  len    = sizeof(buf);
    svc.getStatus(buf, &len);

    EXPECT_EQ(BleOtaService::kStatusLen, len);
    EXPECT_EQ(static_cast<uint8_t>(OtaState::RECEIVING), buf[0]);
    // bytes_received LE32
    EXPECT_EQ(0x00u, buf[1]);
    EXPECT_EQ(0x02u, buf[2]);
    EXPECT_EQ(0x00u, buf[3]);
    EXPECT_EQ(0x00u, buf[4]);
    // last OtaStatus
    EXPECT_EQ(static_cast<uint8_t>(OtaStatus::OK), buf[5]);
    EXPECT_EQ(kOtaTargetIdNrf52dkNrf52832 & 0xFFu, buf[6]);
    EXPECT_EQ((kOtaTargetIdNrf52dkNrf52832 >> 8) & 0xFFu, buf[7]);
    EXPECT_EQ((kOtaTargetIdNrf52dkNrf52832 >> 16) & 0xFFu, buf[8]);
    EXPECT_EQ((kOtaTargetIdNrf52dkNrf52832 >> 24) & 0xFFu, buf[9]);
}

TEST_F(BleOtaServiceTest, GetStatusReflectsLastError) {
    // Trigger an error via a bad commit.
    const uint8_t commit_pkt[] = {0x03};
    EXPECT_CALL(mgr, commit()).WillOnce(Return(OtaStatus::ERROR_INTEGRITY_FAILED));
    svc.handleControlWrite(commit_pkt, sizeof(commit_pkt));

    EXPECT_CALL(mgr, state()).WillOnce(Return(OtaState::IDLE));
    EXPECT_CALL(mgr, bytesReceived()).WillOnce(Return(0u));

    uint8_t buf[BleOtaService::kStatusLen] = {};
    size_t  len    = sizeof(buf);
    svc.getStatus(buf, &len);

    EXPECT_EQ(static_cast<uint8_t>(OtaStatus::ERROR_INTEGRITY_FAILED), buf[5]);
}

TEST_F(BleOtaServiceTest, GetStatusHandlesNullBuffer) {
    size_t len = BleOtaService::kStatusLen;
    svc.getStatus(nullptr, &len);  // must not crash
}
