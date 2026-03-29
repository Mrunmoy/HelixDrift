#pragma once

#include "Quaternion.hpp"
#include "VirtualRFMedium.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace sim::rfsync {

enum class PacketType : uint8_t {
    ANCHOR = 1,
    FRAME = 2,
};

struct AnchorPayload {
    uint32_t sequence = 0;
    uint64_t masterTimestampUs = 0;
};

struct FramePayload {
    uint32_t sequence = 0;
    uint64_t localTimestampUs = 0;
    int64_t estimatedOffsetUs = 0;
    sf::Quaternion orientation{};
};

template <typename T>
inline void appendBytes(std::vector<uint8_t>& bytes, const T& value) {
    const auto* ptr = reinterpret_cast<const uint8_t*>(&value);
    bytes.insert(bytes.end(), ptr, ptr + sizeof(T));
}

template <typename T>
inline bool readBytes(const std::vector<uint8_t>& bytes, size_t offset, T& out) {
    if (offset + sizeof(T) > bytes.size()) {
        return false;
    }
    std::memcpy(&out, bytes.data() + offset, sizeof(T));
    return true;
}

inline std::vector<uint8_t> encodeAnchor(const AnchorPayload& payload) {
    std::vector<uint8_t> bytes;
    bytes.reserve(1u + sizeof(payload.sequence) + sizeof(payload.masterTimestampUs));
    bytes.push_back(static_cast<uint8_t>(PacketType::ANCHOR));
    appendBytes(bytes, payload.sequence);
    appendBytes(bytes, payload.masterTimestampUs);
    return bytes;
}

inline bool decodeAnchor(const Packet& packet, AnchorPayload& payload) {
    if (packet.payload.empty() ||
        packet.payload[0] != static_cast<uint8_t>(PacketType::ANCHOR)) {
        return false;
    }
    return readBytes(packet.payload, 1u, payload.sequence) &&
           readBytes(packet.payload, 1u + sizeof(payload.sequence), payload.masterTimestampUs);
}

inline std::vector<uint8_t> encodeFrame(const FramePayload& payload) {
    std::vector<uint8_t> bytes;
    bytes.reserve(1u + sizeof(payload.sequence) + sizeof(payload.localTimestampUs) +
                  sizeof(payload.estimatedOffsetUs) + sizeof(payload.orientation));
    bytes.push_back(static_cast<uint8_t>(PacketType::FRAME));
    appendBytes(bytes, payload.sequence);
    appendBytes(bytes, payload.localTimestampUs);
    appendBytes(bytes, payload.estimatedOffsetUs);
    appendBytes(bytes, payload.orientation);
    return bytes;
}

inline bool decodeFrame(const Packet& packet, FramePayload& payload) {
    if (packet.payload.empty() ||
        packet.payload[0] != static_cast<uint8_t>(PacketType::FRAME)) {
        return false;
    }

    size_t offset = 1u;
    return readBytes(packet.payload, offset, payload.sequence) &&
           readBytes(packet.payload, offset += sizeof(payload.sequence), payload.localTimestampUs) &&
           readBytes(packet.payload, offset += sizeof(payload.localTimestampUs), payload.estimatedOffsetUs) &&
           readBytes(packet.payload, offset += sizeof(payload.estimatedOffsetUs), payload.orientation);
}

} // namespace sim::rfsync
