#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <cstdint>

namespace {
constexpr uint32_t kLedPin = 17;  // LED1 on nRF52 DK, active low.
constexpr uint32_t kTxPin = 6;    // UART0 TXD -> interface MCU RXD.
constexpr uint32_t kRxPin = 8;    // UART0 RXD <- interface MCU TXD.

#ifndef HELIX_OTA_PROBE_LABEL
#define HELIX_OTA_PROBE_LABEL "unknown"
#endif

#ifndef HELIX_OTA_PROBE_INTERVAL_MS
#define HELIX_OTA_PROBE_INTERVAL_MS 500
#endif

constexpr char kVersionLabel[] = HELIX_OTA_PROBE_LABEL;
constexpr uint32_t kBlinkIntervalMs = HELIX_OTA_PROBE_INTERVAL_MS;
constexpr uintptr_t kUart0Base = 0x40002000u;

struct ProbeStatus {
    uint32_t magic;
    uint32_t bootCount;
    uint32_t tickCount;
    uint32_t lastByte;
};

volatile ProbeStatus g_probeStatus __attribute__((section(".noinit.otaprobe")));

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

static Uart0Regs& uart0() {
    return *reinterpret_cast<Uart0Regs*>(kUart0Base);
}

void ledSet(bool on) {
    if (on) {
        nrf_gpio_pin_clear(kLedPin);
    } else {
        nrf_gpio_pin_set(kLedPin);
    }
}

void uartInit() {
    auto& uart = uart0();
    uart.ENABLE = 0;
    uart.PSELTXD = kTxPin;
    uart.PSELRXD = kRxPin;
    uart.PSELRTS = 0xFFFFFFFFu;
    uart.PSELCTS = 0xFFFFFFFFu;
    uart.BAUDRATE = 0x01D7E000u;  // 115200
    uart.CONFIG = 0;
    uart.ENABLE = 4;
    uart.TASKS_STARTTX = 1;
    uart.TASKS_STARTRX = 1;
}

void uartWriteByte(uint8_t byte) {
    auto& uart = uart0();
    uart.EVENTS_TXDRDY = 0;
    uart.TXD = byte;
    while (uart.EVENTS_TXDRDY == 0) {
    }
    g_probeStatus.lastByte = byte;
}

void uartWrite(const char* s) {
    while (*s) {
        if (*s == '\n') {
            uartWriteByte('\r');
        }
        uartWriteByte(static_cast<uint8_t>(*s++));
    }
}

void uartWriteU32(uint32_t value) {
    char buf[10];
    int i = 0;
    do {
        buf[i++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);
    while (i-- > 0) {
        uartWriteByte(static_cast<uint8_t>(buf[i]));
    }
}
} // namespace

int main() {
    nrf_gpio_cfg_output(kLedPin);
    ledSet(false);
    uartInit();

    if (g_probeStatus.magic != 0x48444F54u) { // "HDOT"
        g_probeStatus.magic = 0x48444F54u;
        g_probeStatus.bootCount = 0u;
        g_probeStatus.tickCount = 0u;
        g_probeStatus.lastByte = 0u;
    }
    ++g_probeStatus.bootCount;

    uartWrite("HelixDrift nRF52 DK OTA probe\n");
    uartWrite("version=");
    uartWrite(kVersionLabel);
    uartWrite(" boot=");
    uartWriteU32(g_probeStatus.bootCount);
    uartWrite("\n");

    bool on = false;
    uint32_t tick = 0;
    while (true) {
        on = !on;
        ledSet(on);
        uartWrite("probe ");
        uartWrite(kVersionLabel);
        uartWrite(" tick ");
        uartWriteU32(tick++);
        uartWrite("\n");
        g_probeStatus.tickCount = tick;
        nrf_delay_ms(kBlinkIntervalMs);
    }
}
