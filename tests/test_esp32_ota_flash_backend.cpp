#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define ESP32_STUB
#include "esp_ota_ops.h"
#include "EspOtaOpsInterface.hpp"
#include "OtaManager.hpp"
#include "Esp32OtaFlashBackend.hpp"

using namespace helix;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

// ---------------------------------------------------------------------------
// Mock
// ---------------------------------------------------------------------------

class MockEspOtaOps : public EspOtaOpsInterface {
public:
    MOCK_METHOD(const esp_partition_t*, getNextUpdatePartition,
                (const esp_partition_t*), (override));
    MOCK_METHOD(esp_err_t, begin,
                (const esp_partition_t*, size_t, esp_ota_handle_t*), (override));
    MOCK_METHOD(esp_err_t, write,
                (esp_ota_handle_t, const void*, size_t), (override));
    MOCK_METHOD(esp_err_t, end, (esp_ota_handle_t), (override));
    MOCK_METHOD(esp_err_t, setBootPartition, (const esp_partition_t*), (override));
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const esp_partition_t kFakePartition{
    .type      = 0,
    .subtype   = 0x10,
    .address   = 0x10000,
    .size      = 0xF0000,
    .label     = "ota_0",
    .encrypted = false,
};
static constexpr esp_ota_handle_t kFakeHandle = 42u;

static constexpr uint32_t kSlotSize = 0xF0000u;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class Esp32OtaFlashBackendTest : public ::testing::Test {
protected:
    StrictMock<MockEspOtaOps> ops;
    Esp32OtaFlashBackend      backend{ops, kSlotSize};

    void expectSuccessfulErase() {
        EXPECT_CALL(ops, getNextUpdatePartition(nullptr))
            .WillOnce(Return(&kFakePartition));
        EXPECT_CALL(ops, begin(&kFakePartition, _, _))
            .WillOnce(DoAll(SetArgPointee<2>(kFakeHandle), Return(ESP_OK)));
    }
};

// ---------------------------------------------------------------------------
// eraseSlot()
// ---------------------------------------------------------------------------

TEST_F(Esp32OtaFlashBackendTest, EraseSlotGetsNextPartitionAndCallsBegin) {
    EXPECT_CALL(ops, getNextUpdatePartition(nullptr))
        .WillOnce(Return(&kFakePartition));
    EXPECT_CALL(ops, begin(&kFakePartition, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(kFakeHandle), Return(ESP_OK)));

    EXPECT_TRUE(backend.eraseSlot());
}

TEST_F(Esp32OtaFlashBackendTest, EraseSlotReturnsFalseWhenNoPartitionAvailable) {
    EXPECT_CALL(ops, getNextUpdatePartition(nullptr))
        .WillOnce(Return(nullptr));

    EXPECT_FALSE(backend.eraseSlot());
}

TEST_F(Esp32OtaFlashBackendTest, EraseSlotReturnsFalseWhenBeginFails) {
    EXPECT_CALL(ops, getNextUpdatePartition(nullptr))
        .WillOnce(Return(&kFakePartition));
    EXPECT_CALL(ops, begin(&kFakePartition, _, _))
        .WillOnce(Return(ESP_FAIL));

    EXPECT_FALSE(backend.eraseSlot());
}

TEST_F(Esp32OtaFlashBackendTest, SecondEraseClosesOldSessionBeforeStartingNew) {
    expectSuccessfulErase();
    EXPECT_TRUE(backend.eraseSlot());

    // Second eraseSlot must close the active handle first.
    EXPECT_CALL(ops, end(kFakeHandle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(ops, getNextUpdatePartition(nullptr))
        .WillOnce(Return(&kFakePartition));
    EXPECT_CALL(ops, begin(&kFakePartition, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(kFakeHandle), Return(ESP_OK)));

    EXPECT_TRUE(backend.eraseSlot());
}

// ---------------------------------------------------------------------------
// slotSize()
// ---------------------------------------------------------------------------

TEST_F(Esp32OtaFlashBackendTest, SlotSizeReturnsConfiguredSize) {
    EXPECT_EQ(kSlotSize, backend.slotSize());
}

TEST_F(Esp32OtaFlashBackendTest, SlotSizeAvailableBeforeErase) {
    EXPECT_GT(backend.slotSize(), 0u);
}

// ---------------------------------------------------------------------------
// writeChunk()
// ---------------------------------------------------------------------------

TEST_F(Esp32OtaFlashBackendTest, WriteChunkForwardsDataToEspOtaWrite) {
    expectSuccessfulErase();
    ASSERT_TRUE(backend.eraseSlot());

    const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_CALL(ops, write(kFakeHandle, data, sizeof(data)))
        .WillOnce(Return(ESP_OK));

    EXPECT_TRUE(backend.writeChunk(0, data, sizeof(data)));
}

TEST_F(Esp32OtaFlashBackendTest, WriteChunkReturnsFalseWhenNoActiveSession) {
    const uint8_t data[] = {0x01};
    EXPECT_FALSE(backend.writeChunk(0, data, sizeof(data)));
}

TEST_F(Esp32OtaFlashBackendTest, WriteChunkReturnsFalseOnEspOtaWriteFailure) {
    expectSuccessfulErase();
    ASSERT_TRUE(backend.eraseSlot());

    const uint8_t data[] = {0xAB};
    EXPECT_CALL(ops, write(kFakeHandle, data, sizeof(data)))
        .WillOnce(Return(ESP_FAIL));

    EXPECT_FALSE(backend.writeChunk(0, data, sizeof(data)));
}

TEST_F(Esp32OtaFlashBackendTest, WriteChunkOffsetIgnoredEspOtaIsSequential) {
    expectSuccessfulErase();
    ASSERT_TRUE(backend.eraseSlot());

    const uint8_t chunk1[] = {0x11, 0x22};
    const uint8_t chunk2[] = {0x33, 0x44};

    EXPECT_CALL(ops, write(kFakeHandle, chunk1, 2u)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(ops, write(kFakeHandle, chunk2, 2u)).WillOnce(Return(ESP_OK));

    EXPECT_TRUE(backend.writeChunk(0,  chunk1, 2u));
    EXPECT_TRUE(backend.writeChunk(2u, chunk2, 2u));
}

// ---------------------------------------------------------------------------
// setPendingUpgrade()
// ---------------------------------------------------------------------------

TEST_F(Esp32OtaFlashBackendTest, SetPendingUpgradeCallsEndThenSetBootPartition) {
    expectSuccessfulErase();
    ASSERT_TRUE(backend.eraseSlot());

    EXPECT_CALL(ops, end(kFakeHandle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(ops, setBootPartition(&kFakePartition)).WillOnce(Return(ESP_OK));

    EXPECT_TRUE(backend.setPendingUpgrade());
}

TEST_F(Esp32OtaFlashBackendTest, SetPendingUpgradeReturnsFalseWithNoSession) {
    EXPECT_FALSE(backend.setPendingUpgrade());
}

TEST_F(Esp32OtaFlashBackendTest, SetPendingUpgradeReturnsFalseWhenEndFails) {
    expectSuccessfulErase();
    ASSERT_TRUE(backend.eraseSlot());

    EXPECT_CALL(ops, end(kFakeHandle)).WillOnce(Return(ESP_FAIL));

    EXPECT_FALSE(backend.setPendingUpgrade());
}

TEST_F(Esp32OtaFlashBackendTest, SetPendingUpgradeReturnsFalseWhenSetBootFails) {
    expectSuccessfulErase();
    ASSERT_TRUE(backend.eraseSlot());

    EXPECT_CALL(ops, end(kFakeHandle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(ops, setBootPartition(&kFakePartition)).WillOnce(Return(ESP_FAIL));

    EXPECT_FALSE(backend.setPendingUpgrade());
}

TEST_F(Esp32OtaFlashBackendTest, SetPendingUpgradeClearsSessionSoWriteChunkFails) {
    expectSuccessfulErase();
    ASSERT_TRUE(backend.eraseSlot());

    EXPECT_CALL(ops, end(kFakeHandle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(ops, setBootPartition(&kFakePartition)).WillOnce(Return(ESP_OK));
    ASSERT_TRUE(backend.setPendingUpgrade());

    const uint8_t data[] = {0x01};
    EXPECT_FALSE(backend.writeChunk(0, data, sizeof(data)));
}

// ---------------------------------------------------------------------------
// Full round-trip via OtaManager
// ---------------------------------------------------------------------------

TEST_F(Esp32OtaFlashBackendTest, FullOtaRoundTripSucceeds) {
    const uint8_t image[] = {0x01, 0x02, 0x03, 0x04};
    constexpr uint32_t kSize = sizeof(image);

    // CRC32 helper matching OtaManager's IEEE 802.3 algorithm.
    auto crc32 = [](const uint8_t* data, size_t len) -> uint32_t {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int b = 0; b < 8; ++b)
                crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
        }
        return crc ^ 0xFFFFFFFFu;
    };

    EXPECT_CALL(ops, getNextUpdatePartition(nullptr))
        .WillOnce(Return(&kFakePartition));
    EXPECT_CALL(ops, begin(&kFakePartition, _, _))
        .WillOnce(DoAll(SetArgPointee<2>(kFakeHandle), Return(ESP_OK)));
    EXPECT_CALL(ops, write(kFakeHandle, _, kSize))
        .WillOnce(Return(ESP_OK));
    EXPECT_CALL(ops, end(kFakeHandle)).WillOnce(Return(ESP_OK));
    EXPECT_CALL(ops, setBootPartition(&kFakePartition)).WillOnce(Return(ESP_OK));

    OtaManager manager{backend};
    EXPECT_EQ(OtaStatus::OK, manager.begin(kSize, crc32(image, kSize)));
    EXPECT_EQ(OtaStatus::OK, manager.writeChunk(0, image, kSize));
    EXPECT_EQ(OtaStatus::OK, manager.commit());
}

