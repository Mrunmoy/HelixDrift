#include "BleOtaService.hpp"
#include "IOtaManager.hpp"
#include "NrfOtaFlashBackend.hpp"
#include "UartOtaProtocol.hpp"
#include "nrf_gpio.h"
#include <cstddef>
#include <cstdint>

#ifndef HELIX_OTA_PROBE_LABEL
#define HELIX_OTA_PROBE_LABEL "ota"
#endif

#ifndef HELIX_OTA_PROBE_INTERVAL_MS
#define HELIX_OTA_PROBE_INTERVAL_MS 500u
#endif

namespace {
constexpr uint32_t kLedPin = 17u;   // LED1 on nRF52 DK, active low.
constexpr uint32_t kTxPin = 6u;     // DK VCOM TX.
constexpr uint32_t kRxPin = 8u;     // DK VCOM RX.
constexpr uint32_t kCoreClockHz = 64000000u;
constexpr uintptr_t kUart0Base = 0x40002000u;
constexpr uintptr_t kDemcrAddr = 0xE000EDFCu;
constexpr uintptr_t kDwtCtrlAddr = 0xE0001000u;
constexpr uintptr_t kDwtCyccntAddr = 0xE0001004u;
constexpr uintptr_t kAircrAddr = 0xE000ED0Cu;
constexpr uint32_t kDemcrTrcena = 1u << 24;
constexpr uint32_t kDwtCtrlCyccntena = 1u << 0;
constexpr uint32_t kAircrVkey = 0x05FAu << 16;
constexpr uint32_t kAircrSysResetReq = 1u << 2;
constexpr uint32_t kDkSecondarySlotBase = 0x0003C000u;
constexpr uint32_t kDkSecondarySlotSize = 0x00024000u;
constexpr uint32_t kResetDelayMs = 200u;
constexpr size_t kMaxPayload = 192u;
constexpr size_t kMaxFrameLen = helix::UartOtaProtocol::kFrameOverhead + kMaxPayload;

struct Uart0Regs {
    volatile uint32_t TASKS_STARTRX;
    volatile uint32_t TASKS_STOPRX;
    volatile uint32_t TASKS_STARTTX;
    volatile uint32_t TASKS_STOPTX;
    uint32_t _reserved0[3];
    volatile uint32_t TASKS_SUSPEND;
    uint32_t _reserved1[56];
    volatile uint32_t EVENTS_CTS;
    volatile uint32_t EVENTS_NCTS;
    volatile uint32_t EVENTS_RXDRDY;
    uint32_t _reserved2[4];
    volatile uint32_t EVENTS_TXDRDY;
    uint32_t _reserved3[1];
    volatile uint32_t EVENTS_ERROR;
    uint32_t _reserved4[7];
    volatile uint32_t EVENTS_RXTO;
    uint32_t _reserved5[46];
    volatile uint32_t SHORTS;
    uint32_t _reserved6[64];
    volatile uint32_t INTENSET;
    volatile uint32_t INTENCLR;
    uint32_t _reserved7[93];
    volatile uint32_t ERRORSRC;
    uint32_t _reserved8[31];
    volatile uint32_t ENABLE;
    uint32_t _reserved9[1];
    volatile uint32_t PSELRTS;
    volatile uint32_t PSELTXD;
    volatile uint32_t PSELCTS;
    volatile uint32_t PSELRXD;
    volatile uint32_t RXD;
    volatile uint32_t TXD;
    uint32_t _reserved10[1];
    volatile uint32_t BAUDRATE;
    uint32_t _reserved11[17];
    volatile uint32_t CONFIG;
};

struct OtaSerialStatus {
    uint32_t magic;
    uint32_t framesRx;
    uint32_t framesTx;
    uint32_t lastType;
    uint32_t lastStatus;
    uint32_t lastBytesReceived;
    uint32_t resetScheduled;
};

volatile OtaSerialStatus g_status __attribute__((section(".noinit.ota_uart")));

static Uart0Regs& uart0() {
    return *reinterpret_cast<Uart0Regs*>(kUart0Base);
}

volatile uint32_t& reg32(uintptr_t addr) {
    return *reinterpret_cast<volatile uint32_t*>(addr);
}

uint32_t readU32Le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8u)
         | (static_cast<uint32_t>(p[2]) << 16u)
         | (static_cast<uint32_t>(p[3]) << 24u);
}

void writeU32Le(uint8_t* p, uint32_t value) {
    p[0] = static_cast<uint8_t>(value & 0xFFu);
    p[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
    p[2] = static_cast<uint8_t>((value >> 16u) & 0xFFu);
    p[3] = static_cast<uint8_t>((value >> 24u) & 0xFFu);
}

void ledSet(bool on) {
    if (on) {
        nrf_gpio_pin_clear(kLedPin);
    } else {
        nrf_gpio_pin_set(kLedPin);
    }
}

void delayInit() {
    reg32(kDemcrAddr) |= kDemcrTrcena;
    reg32(kDwtCyccntAddr) = 0u;
    reg32(kDwtCtrlAddr) |= kDwtCtrlCyccntena;
}

uint32_t cyclesNow() {
    return reg32(kDwtCyccntAddr);
}

uint32_t msToCycles(uint32_t ms) {
    return ms * (kCoreClockHz / 1000u);
}

bool elapsed(uint32_t startCycles, uint32_t durationCycles) {
    return static_cast<uint32_t>(cyclesNow() - startCycles) >= durationCycles;
}

void requestSystemReset() {
    reg32(kAircrAddr) = kAircrVkey | kAircrSysResetReq;
    while (true) {
    }
}

void uartInit() {
    auto& uart = uart0();
    uart.ENABLE = 0u;
    uart.PSELTXD = kTxPin;
    uart.PSELRXD = kRxPin;
    uart.PSELRTS = 0xFFFFFFFFu;
    uart.PSELCTS = 0xFFFFFFFFu;
    uart.BAUDRATE = 0x01D7E000u; // 115200
    uart.CONFIG = 0u;
    uart.ENABLE = 4u;
    uart.TASKS_STARTTX = 1u;
    uart.TASKS_STARTRX = 1u;
}

void uartWriteByte(uint8_t byte) {
    auto& uart = uart0();
    uart.EVENTS_TXDRDY = 0u;
    uart.TXD = byte;
    while (uart.EVENTS_TXDRDY == 0u) {
    }
}

void uartWriteFrame(const uint8_t* frame, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        uartWriteByte(frame[i]);
    }
    ++g_status.framesTx;
}

bool uartReadByte(uint8_t& out) {
    auto& uart = uart0();
    if (uart.EVENTS_RXDRDY == 0u) {
        return false;
    }
    uart.EVENTS_RXDRDY = 0u;
    out = static_cast<uint8_t>(uart.RXD & 0xFFu);
    return true;
}

uint8_t currentLastStatus(const helix::BleOtaService& service) {
    uint8_t status[helix::BleOtaService::kStatusLen] = {};
    size_t len = 0u;
    service.getStatus(status, &len);
    (void)len;
    return status[5];
}

void sendInfoResponse(const helix::BleOtaService& service,
                      const helix::IOtaManager& manager,
                      uint8_t* payload,
                      uint8_t* frame) {
    uint8_t status[helix::BleOtaService::kStatusLen] = {};
    size_t statusLen = 0u;
    service.getStatus(status, &statusLen);
    payload[0] = 1u; // protocol version
    payload[1] = status[0];
    payload[2] = status[5];
    writeU32Le(payload + 3, manager.bytesReceived());
    writeU32Le(payload + 7, kDkSecondarySlotSize);
    writeU32Le(payload + 11, HELIX_OTA_PROBE_INTERVAL_MS);
    constexpr const char* kVersion = HELIX_OTA_PROBE_LABEL;
    constexpr size_t kVersionLen = sizeof(HELIX_OTA_PROBE_LABEL) - 1u;
    payload[15] = static_cast<uint8_t>(kVersionLen);
    for (size_t i = 0; i < kVersionLen; ++i) {
        payload[16 + i] = static_cast<uint8_t>(kVersion[i]);
    }
    size_t frameLen = 0u;
    helix::UartOtaProtocol::encode(
        static_cast<uint8_t>(helix::UartOtaProtocol::FrameType::InfoRsp),
        payload,
        16u + kVersionLen,
        frame,
        kMaxFrameLen,
        frameLen);
    uartWriteFrame(frame, frameLen);
}

void sendStatusResponse(const helix::BleOtaService& service,
                        uint8_t* payload,
                        uint8_t* frame) {
    uint8_t status[helix::BleOtaService::kStatusLen] = {};
    size_t statusLen = 0u;
    service.getStatus(status, &statusLen);
    (void)statusLen;
    payload[0] = status[0];
    payload[1] = status[5];
    writeU32Le(payload + 2, readU32Le(status + 1));
    size_t frameLen = 0u;
    helix::UartOtaProtocol::encode(
        static_cast<uint8_t>(helix::UartOtaProtocol::FrameType::StatusRsp),
        payload,
        6u,
        frame,
        kMaxFrameLen,
        frameLen);
    uartWriteFrame(frame, frameLen);
}

void sendCtrlResponse(helix::OtaStatus status, uint8_t* payload, uint8_t* frame) {
    payload[0] = static_cast<uint8_t>(status);
    size_t frameLen = 0u;
    helix::UartOtaProtocol::encode(
        static_cast<uint8_t>(helix::UartOtaProtocol::FrameType::CtrlRsp),
        payload,
        1u,
        frame,
        kMaxFrameLen,
        frameLen);
    uartWriteFrame(frame, frameLen);
}

void sendDataResponse(helix::OtaStatus status,
                      const helix::IOtaManager& manager,
                      uint8_t* payload,
                      uint8_t* frame) {
    payload[0] = static_cast<uint8_t>(status);
    writeU32Le(payload + 1, manager.bytesReceived());
    size_t frameLen = 0u;
    helix::UartOtaProtocol::encode(
        static_cast<uint8_t>(helix::UartOtaProtocol::FrameType::DataRsp),
        payload,
        5u,
        frame,
        kMaxFrameLen,
        frameLen);
    uartWriteFrame(frame, frameLen);
}
} // namespace

int main() {
    nrf_gpio_cfg_output(kLedPin);
    ledSet(false);
    delayInit();
    uartInit();

    g_status.magic = 0x48554F37u; // "HUO7"
    g_status.framesRx = 0u;
    g_status.framesTx = 0u;
    g_status.lastType = 0u;
    g_status.lastStatus = 0u;
    g_status.lastBytesReceived = 0u;
    g_status.resetScheduled = 0u;

    helix::NrfOtaFlashBackend backend{kDkSecondarySlotBase, kDkSecondarySlotSize};
    helix::OtaManager manager{backend};
    helix::OtaManagerAdapter managerAdapter{manager};
    helix::BleOtaService service{managerAdapter};
    helix::UartOtaFrameParser<kMaxPayload> parser;
    helix::UartOtaMutableFrame decoded{};

    uint8_t responsePayload[kMaxPayload] = {};
    uint8_t responseFrame[helix::UartOtaProtocol::kFrameOverhead + kMaxPayload] = {};

    bool ledOn = false;
    uint32_t lastBlink = cyclesNow();
    bool resetPending = false;
    uint32_t resetStarted = 0u;

    while (true) {
        if (elapsed(lastBlink, msToCycles(HELIX_OTA_PROBE_INTERVAL_MS))) {
            ledOn = !ledOn;
            ledSet(ledOn);
            lastBlink = cyclesNow();
        }

        if (resetPending && elapsed(resetStarted, msToCycles(kResetDelayMs))) {
            requestSystemReset();
        }

        uint8_t byte = 0u;
        if (!uartReadByte(byte)) {
            continue;
        }

        if (!parser.push(byte, decoded)) {
            continue;
        }

        ++g_status.framesRx;
        g_status.lastType = decoded.type;

        using FrameType = helix::UartOtaProtocol::FrameType;
        helix::OtaStatus status = helix::OtaStatus::OK;

        switch (static_cast<FrameType>(decoded.type)) {
            case FrameType::InfoReq:
                sendInfoResponse(service, managerAdapter, responsePayload, responseFrame);
                break;

            case FrameType::StatusReq:
                sendStatusResponse(service, responsePayload, responseFrame);
                break;

            case FrameType::CtrlWrite:
                if (decoded.payloadLen == 0u) {
                    status = helix::OtaStatus::ERROR_INVALID_STATE;
                } else {
                    status = service.handleControlWrite(decoded.payload, decoded.payloadLen);
                }
                g_status.lastStatus = static_cast<uint32_t>(status);
                g_status.lastBytesReceived = manager.bytesReceived();
                sendCtrlResponse(status, responsePayload, responseFrame);
                if (status == helix::OtaStatus::OK && decoded.payload[0] == helix::BleOtaService::CMD_COMMIT) {
                    resetPending = true;
                    resetStarted = cyclesNow();
                    g_status.resetScheduled = 1u;
                }
                break;

            case FrameType::DataWrite:
                status = service.handleDataWrite(decoded.payload, decoded.payloadLen);
                g_status.lastStatus = static_cast<uint32_t>(status);
                g_status.lastBytesReceived = manager.bytesReceived();
                sendDataResponse(status, managerAdapter, responsePayload, responseFrame);
                break;

            default:
                status = helix::OtaStatus::ERROR_INVALID_STATE;
                g_status.lastStatus = static_cast<uint32_t>(status);
                sendCtrlResponse(status, responsePayload, responseFrame);
                break;
        }
    }
}
