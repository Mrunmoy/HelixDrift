#pragma once

#include <cstddef>
#include <cstdint>

namespace helix {

struct UartOtaFrame {
    uint8_t type = 0;
    const uint8_t* payload = nullptr;
    std::size_t payloadLen = 0;
};

struct UartOtaMutableFrame {
    uint8_t type = 0;
    uint8_t* payload = nullptr;
    std::size_t payloadLen = 0;
};

struct UartOtaProtocol {
    static constexpr uint8_t kSof0 = 0x48u; // 'H'
    static constexpr uint8_t kSof1 = 0x44u; // 'D'
    static constexpr std::size_t kHeaderSize = 5u;
    static constexpr std::size_t kFooterSize = 1u;
    static constexpr std::size_t kFrameOverhead = kHeaderSize + kFooterSize;

    enum class FrameType : uint8_t {
        InfoReq      = 0x10u,
        InfoRsp      = 0x11u,
        CtrlWrite    = 0x20u,
        CtrlRsp      = 0x21u,
        DataWrite    = 0x30u,
        DataRsp      = 0x31u,
        StatusReq    = 0x40u,
        StatusRsp    = 0x41u,
        /* Hub-relay-only: request Hub to send an ESB OTA trigger to a Tag
         * so it reboots into its BLE OTA window. Payload: [node_id, retries] */
        EsbTrigReq   = 0x50u,
        EsbTrigRsp   = 0x51u,
    };

    static uint8_t checksum(uint8_t type, const uint8_t* payload, std::size_t payloadLen) {
        uint32_t sum = type;
        sum += static_cast<uint32_t>(payloadLen & 0xFFu);
        sum += static_cast<uint32_t>((payloadLen >> 8u) & 0xFFu);
        for (std::size_t i = 0; i < payloadLen; ++i) {
            sum += payload[i];
        }
        return static_cast<uint8_t>(sum & 0xFFu);
    }

    static bool encode(
        uint8_t type,
        const uint8_t* payload,
        std::size_t payloadLen,
        uint8_t* out,
        std::size_t outCapacity,
        std::size_t& outLen) {
        const std::size_t totalLen = kFrameOverhead + payloadLen;
        if (out == nullptr || outCapacity < totalLen || payloadLen > 0xFFFFu) {
            return false;
        }

        out[0] = kSof0;
        out[1] = kSof1;
        out[2] = type;
        out[3] = static_cast<uint8_t>(payloadLen & 0xFFu);
        out[4] = static_cast<uint8_t>((payloadLen >> 8u) & 0xFFu);
        for (std::size_t i = 0; i < payloadLen; ++i) {
            out[kHeaderSize + i] = payload[i];
        }
        out[kHeaderSize + payloadLen] = checksum(type, payload, payloadLen);
        outLen = totalLen;
        return true;
    }

    static bool decode(const uint8_t* frame, std::size_t frameLen, UartOtaFrame& out) {
        if (frame == nullptr || frameLen < kFrameOverhead) {
            return false;
        }
        if (frame[0] != kSof0 || frame[1] != kSof1) {
            return false;
        }
        const std::size_t payloadLen =
            static_cast<std::size_t>(frame[3]) |
            (static_cast<std::size_t>(frame[4]) << 8u);
        if (frameLen != kFrameOverhead + payloadLen) {
            return false;
        }
        const uint8_t expected = checksum(frame[2], frame + kHeaderSize, payloadLen);
        if (frame[kHeaderSize + payloadLen] != expected) {
            return false;
        }
        out.type = frame[2];
        out.payload = frame + kHeaderSize;
        out.payloadLen = payloadLen;
        return true;
    }
};

template <std::size_t MaxPayload>
class UartOtaFrameParser {
public:
    bool push(uint8_t byte, UartOtaMutableFrame& out) {
        switch (state_) {
            case State::SeekSof0:
                if (byte == UartOtaProtocol::kSof0) {
                    buffer_[0] = byte;
                    index_ = 1u;
                    state_ = State::SeekSof1;
                }
                break;

            case State::SeekSof1:
                if (byte == UartOtaProtocol::kSof1) {
                    buffer_[1] = byte;
                    index_ = 2u;
                    state_ = State::Header;
                } else if (byte == UartOtaProtocol::kSof0) {
                    buffer_[0] = byte;
                    index_ = 1u;
                } else {
                    reset();
                }
                break;

            case State::Header:
                buffer_[index_++] = byte;
                if (index_ == UartOtaProtocol::kHeaderSize) {
                    payloadLen_ =
                        static_cast<std::size_t>(buffer_[3]) |
                        (static_cast<std::size_t>(buffer_[4]) << 8u);
                    if (payloadLen_ > MaxPayload) {
                        reset();
                        break;
                    }
                    state_ = payloadLen_ == 0u ? State::Checksum : State::Payload;
                }
                break;

            case State::Payload:
                buffer_[index_++] = byte;
                if (index_ == UartOtaProtocol::kHeaderSize + payloadLen_) {
                    state_ = State::Checksum;
                }
                break;

            case State::Checksum:
                buffer_[index_++] = byte;
                {
                    UartOtaFrame decoded{};
                    const bool ok = UartOtaProtocol::decode(buffer_, index_, decoded);
                    if (ok) {
                        out.type = decoded.type;
                        out.payload = buffer_ + UartOtaProtocol::kHeaderSize;
                        out.payloadLen = decoded.payloadLen;
                        reset();
                        return true;
                    }
                }
                reset();
                break;
        }

        return false;
    }

    void reset() {
        state_ = State::SeekSof0;
        index_ = 0u;
        payloadLen_ = 0u;
    }

private:
    enum class State : uint8_t {
        SeekSof0,
        SeekSof1,
        Header,
        Payload,
        Checksum,
    };

    State state_ = State::SeekSof0;
    std::size_t index_ = 0u;
    std::size_t payloadLen_ = 0u;
    uint8_t buffer_[UartOtaProtocol::kFrameOverhead + MaxPayload] = {};
};

} // namespace helix
