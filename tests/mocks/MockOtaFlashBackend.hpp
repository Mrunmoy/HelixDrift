#pragma once

#include <gmock/gmock.h>
#include "OtaFlashBackend.hpp"

namespace helix {
namespace test {

class MockOtaFlashBackend : public OtaFlashBackend {
public:
    MOCK_METHOD(bool, eraseSlot, (), (override));
    MOCK_METHOD(bool, writeChunk, (uint32_t offset, const uint8_t* data, size_t len), (override));
    MOCK_METHOD(bool, setPendingUpgrade, (), (override));
    MOCK_METHOD(uint32_t, slotSize, (), (const, override));
};

} // namespace test
} // namespace helix
