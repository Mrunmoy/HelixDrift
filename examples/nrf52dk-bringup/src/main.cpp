#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <cstdint>

namespace {
constexpr uint32_t kLedPin = 17;  // LED1 on nRF52 DK, active low.
constexpr uint32_t kLed2Pin = 18; // LED2 on nRF52 DK, active low. Keep forced off.
// nRF52 DK virtual COM port routing from the Nordic DK user guide.
constexpr uint32_t kTxPin = 6;    // UART0 TXD -> interface MCU RXD.
constexpr uint32_t kRxPin = 8;    // UART0 RXD <- interface MCU TXD.
constexpr uint32_t kStatusMagic = 0x48445537u; // "HDU7"

constexpr uintptr_t kUart0Base = 0x40002000u;

enum class Phase : uint32_t {
    Boot = 0,
    UartInit = 1,
    BannerWrite = 2,
    Tick = 3,
    Delay = 4,
};

struct BringupStatus {
    uint32_t magic;
    uint32_t phase;
    uint32_t heartbeat;
    uint32_t bannerWrites;
    uint32_t tickWrites;
    uint32_t lastByte;
    uint32_t txReadyCount;
    uint32_t txReadyStalls;
    uint32_t lastErrorEvent;
    uint32_t lastErrorSrc;
    uint32_t uartEnable;
    uint32_t pselTxd;
    uint32_t pselRxd;
    uint32_t baudrate;
};

volatile BringupStatus g_bringupStatus __attribute__((section(".noinit.bringup")));

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

void setPhase(Phase phase) {
    g_bringupStatus.phase = static_cast<uint32_t>(phase);
}

void driveLed(bool ledActive) {
    if (ledActive) nrf_gpio_pin_clear(kLedPin);
    else nrf_gpio_pin_set(kLedPin);
}

void forceLed2Off() {
    nrf_gpio_pin_set(kLed2Pin);
}

void uartInit() {
    auto& uart = uart0();
    setPhase(Phase::UartInit);
    uart.ENABLE = 0;
    uart.PSELTXD = kTxPin;
    uart.PSELRXD = kRxPin;
    uart.PSELRTS = 0xFFFFFFFFu;
    uart.PSELCTS = 0xFFFFFFFFu;
    uart.BAUDRATE = 0x01D7E000u; // 115200
    uart.CONFIG = 0;
    uart.ENABLE = 4; // UART enable
    uart.TASKS_STARTTX = 1;
    uart.TASKS_STARTRX = 1;

    g_bringupStatus.uartEnable = uart.ENABLE;
    g_bringupStatus.pselTxd = uart.PSELTXD;
    g_bringupStatus.pselRxd = uart.PSELRXD;
    g_bringupStatus.baudrate = uart.BAUDRATE;
}

void uartWriteByte(uint8_t byte) {
    auto& uart = uart0();
    uart.EVENTS_TXDRDY = 0;
    uart.EVENTS_ERROR = 0;
    uart.TXD = byte;
    uint32_t spins = 0;
    while (uart.EVENTS_TXDRDY == 0) {
        ++spins;
        if (uart.EVENTS_ERROR != 0) {
            g_bringupStatus.lastErrorEvent = uart.EVENTS_ERROR;
            g_bringupStatus.lastErrorSrc = uart.ERRORSRC;
        }
    }
    g_bringupStatus.lastByte = byte;
    g_bringupStatus.txReadyCount += 1u;
    g_bringupStatus.txReadyStalls += spins;
}

void uartWrite(const char* s) {
    while (*s) {
        if (*s == '\n') uartWriteByte('\r');
        uartWriteByte(static_cast<uint8_t>(*s++));
    }
}

} // namespace

int main() {
    nrf_gpio_cfg_output(kLedPin);
    nrf_gpio_cfg_output(kLed2Pin);
    driveLed(false);
    forceLed2Off();

    g_bringupStatus.magic = 0u;
    g_bringupStatus.phase = 0u;
    g_bringupStatus.heartbeat = 0u;
    g_bringupStatus.bannerWrites = 0u;
    g_bringupStatus.tickWrites = 0u;
    g_bringupStatus.lastByte = 0u;
    g_bringupStatus.txReadyCount = 0u;
    g_bringupStatus.txReadyStalls = 0u;
    g_bringupStatus.lastErrorEvent = 0u;
    g_bringupStatus.lastErrorSrc = 0u;
    g_bringupStatus.uartEnable = 0u;
    g_bringupStatus.pselTxd = 0u;
    g_bringupStatus.pselRxd = 0u;
    g_bringupStatus.baudrate = 0u;
    g_bringupStatus.magic = kStatusMagic;
    setPhase(Phase::Boot);

    uartInit();
    setPhase(Phase::BannerWrite);
    uartWrite("HelixDrift nRF52 DK bring-up app\n");
    ++g_bringupStatus.bannerWrites;
    uartWrite("Target: nRF52832 DK / VCOM / LED1 heartbeat\n");
    ++g_bringupStatus.bannerWrites;

    bool on = false;
    while (true) {
        on = !on;
        setPhase(Phase::Tick);
        driveLed(on);
        forceLed2Off();
        uartWrite("tick ");
        uartWriteByte(static_cast<uint8_t>('0' + (g_bringupStatus.heartbeat % 10u)));
        uartWrite("\n");
        ++g_bringupStatus.tickWrites;
        ++g_bringupStatus.heartbeat;
        setPhase(Phase::Delay);
        nrf_delay_ms(250u);
    }
}
