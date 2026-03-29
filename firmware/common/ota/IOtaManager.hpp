#pragma once

#include "OtaManager.hpp"

namespace helix {

/**
 * Abstract interface for OtaManager, used to inject a mock in BleOtaService tests.
 *
 * OtaManager itself does not inherit from this to keep its implementation clean.
 * BleOtaService depends only on IOtaManager, so it is fully testable without a
 * real flash backend.
 */
class IOtaManager {
public:
    virtual ~IOtaManager() = default;

    virtual OtaStatus begin(uint32_t imageSize, uint32_t expectedCrc32) = 0;
    virtual OtaStatus writeChunk(uint32_t offset, const uint8_t* data, size_t len) = 0;
    virtual OtaStatus commit() = 0;
    virtual void      abort() = 0;
    virtual OtaState  state() const = 0;
    virtual uint32_t  bytesReceived() const = 0;
};

/**
 * Adapter that wraps a concrete OtaManager as an IOtaManager.
 * Used in production code to pass OtaManager to BleOtaService.
 */
class OtaManagerAdapter final : public IOtaManager {
public:
    explicit OtaManagerAdapter(OtaManager& mgr) : m_mgr(mgr) {}

    OtaStatus begin(uint32_t imageSize, uint32_t expectedCrc32) override {
        return m_mgr.begin(imageSize, expectedCrc32);
    }
    OtaStatus writeChunk(uint32_t offset, const uint8_t* data, size_t len) override {
        return m_mgr.writeChunk(offset, data, len);
    }
    OtaStatus commit() override { return m_mgr.commit(); }
    void      abort()  override { m_mgr.abort(); }
    OtaState  state()  const override { return m_mgr.state(); }
    uint32_t  bytesReceived() const override { return m_mgr.bytesReceived(); }

private:
    OtaManager& m_mgr;
};

} // namespace helix
