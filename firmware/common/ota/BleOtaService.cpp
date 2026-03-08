#include "BleOtaService.hpp"

namespace helix {

uint32_t BleOtaService::readU32Le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | static_cast<uint32_t>(p[1]) << 8u
         | static_cast<uint32_t>(p[2]) << 16u
         | static_cast<uint32_t>(p[3]) << 24u;
}

OtaStatus BleOtaService::handleControlWrite(const uint8_t* data, size_t len) {
    if (!data || len == 0u) {
        return OtaStatus::ERROR_INVALID_STATE;
    }

    OtaStatus status = OtaStatus::OK;

    switch (data[0]) {
        case CMD_BEGIN:
            if (len < kCtrlBeginMinLen) {
                return OtaStatus::ERROR_BAD_OFFSET;
            }
            status = m_mgr.begin(readU32Le(data + 1), readU32Le(data + 5));
            break;

        case CMD_ABORT:
            m_mgr.abort();
            break;

        case CMD_COMMIT:
            status = m_mgr.commit();
            break;

        default:
            return OtaStatus::ERROR_INVALID_STATE;
    }

    m_lastStatus = status;
    return status;
}

OtaStatus BleOtaService::handleDataWrite(const uint8_t* data, size_t len) {
    if (!data || len < kDataHeaderLen) {
        return OtaStatus::ERROR_INVALID_STATE;
    }

    const uint32_t offset  = readU32Le(data);
    const uint8_t* payload = data + kDataHeaderLen;
    const size_t   payLen  = len - kDataHeaderLen;

    const OtaStatus status = m_mgr.writeChunk(offset, payload, payLen);
    m_lastStatus = status;
    return status;
}

void BleOtaService::getStatus(uint8_t* out, size_t* outLen) const {
    if (!out) return;

    const OtaState state = m_mgr.state();
    const uint32_t rx    = m_mgr.bytesReceived();

    out[0] = static_cast<uint8_t>(state);
    out[1] = static_cast<uint8_t>(rx        & 0xFFu);
    out[2] = static_cast<uint8_t>(rx >> 8u  & 0xFFu);
    out[3] = static_cast<uint8_t>(rx >> 16u & 0xFFu);
    out[4] = static_cast<uint8_t>(rx >> 24u & 0xFFu);
    out[5] = static_cast<uint8_t>(m_lastStatus);

    if (outLen) *outLen = kStatusLen;
}

} // namespace helix
