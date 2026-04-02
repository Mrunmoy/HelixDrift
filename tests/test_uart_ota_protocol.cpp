#include <gtest/gtest.h>

#include "UartOtaProtocol.hpp"

using helix::UartOtaFrame;
using helix::UartOtaMutableFrame;
using helix::UartOtaProtocol;

TEST(UartOtaProtocolTest, EncodesAndDecodesFrame) {
    const uint8_t payload[] = {0x01u, 0x02u, 0xA5u};
    uint8_t frame[32] = {};
    std::size_t frameLen = 0u;

    ASSERT_TRUE(UartOtaProtocol::encode(
        static_cast<uint8_t>(UartOtaProtocol::FrameType::CtrlWrite),
        payload,
        sizeof(payload),
        frame,
        sizeof(frame),
        frameLen));

    UartOtaFrame decoded{};
    ASSERT_TRUE(UartOtaProtocol::decode(frame, frameLen, decoded));
    EXPECT_EQ(decoded.type, static_cast<uint8_t>(UartOtaProtocol::FrameType::CtrlWrite));
    ASSERT_EQ(decoded.payloadLen, sizeof(payload));
    for (std::size_t i = 0; i < sizeof(payload); ++i) {
        EXPECT_EQ(decoded.payload[i], payload[i]);
    }
}

TEST(UartOtaProtocolTest, RejectsChecksumMismatch) {
    const uint8_t payload[] = {0x10u, 0x20u};
    uint8_t frame[16] = {};
    std::size_t frameLen = 0u;

    ASSERT_TRUE(UartOtaProtocol::encode(
        static_cast<uint8_t>(UartOtaProtocol::FrameType::StatusRsp),
        payload,
        sizeof(payload),
        frame,
        sizeof(frame),
        frameLen));

    frame[frameLen - 1u] ^= 0xFFu;
    UartOtaFrame decoded{};
    EXPECT_FALSE(UartOtaProtocol::decode(frame, frameLen, decoded));
}

TEST(UartOtaProtocolTest, ParserSkipsNoiseAndFindsFrame) {
    helix::UartOtaFrameParser<32> parser;
    UartOtaMutableFrame frame{};

    const uint8_t payload[] = {0xABu, 0xCDu, 0xEFu};
    uint8_t encoded[32] = {};
    std::size_t encodedLen = 0u;
    ASSERT_TRUE(UartOtaProtocol::encode(
        static_cast<uint8_t>(UartOtaProtocol::FrameType::InfoReq),
        payload,
        sizeof(payload),
        encoded,
        sizeof(encoded),
        encodedLen));

    EXPECT_FALSE(parser.push(0x00u, frame));
    EXPECT_FALSE(parser.push(0xFFu, frame));

    bool gotFrame = false;
    for (std::size_t i = 0; i < encodedLen; ++i) {
        if (parser.push(encoded[i], frame)) {
            gotFrame = true;
            break;
        }
    }

    ASSERT_TRUE(gotFrame);
    EXPECT_EQ(frame.type, static_cast<uint8_t>(UartOtaProtocol::FrameType::InfoReq));
    ASSERT_EQ(frame.payloadLen, sizeof(payload));
    for (std::size_t i = 0; i < sizeof(payload); ++i) {
        EXPECT_EQ(frame.payload[i], payload[i]);
    }
}

TEST(UartOtaProtocolTest, ParserRejectsOversizedPayload) {
    helix::UartOtaFrameParser<4> parser;
    UartOtaMutableFrame frame{};

    uint8_t encoded[32] = {};
    std::size_t encodedLen = 0u;
    const uint8_t payload[] = {1u, 2u, 3u, 4u, 5u};
    ASSERT_TRUE(UartOtaProtocol::encode(
        static_cast<uint8_t>(UartOtaProtocol::FrameType::DataWrite),
        payload,
        sizeof(payload),
        encoded,
        sizeof(encoded),
        encodedLen));

    for (std::size_t i = 0; i < encodedLen; ++i) {
        EXPECT_FALSE(parser.push(encoded[i], frame));
    }
}
