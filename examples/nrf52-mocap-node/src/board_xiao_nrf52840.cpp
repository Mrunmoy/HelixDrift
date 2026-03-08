#include "board_xiao_nrf52840.hpp"
#include "NrfOtaFlashBackend.hpp"
#include "OtaManager.hpp"

namespace {
// Seeed XIAO BLE (nRF52840) dual-I2C mapping used by README wiring guide.
constexpr uint32_t kTwim0SdaPin = 4;   // D4 / P0.04
constexpr uint32_t kTwim0SclPin = 5;   // D5 / P0.05
constexpr uint32_t kTwim1SdaPin = 43;  // D6 / P1.11
constexpr uint32_t kTwim1SclPin = 44;  // D7 / P1.12

// OTA manager — single static instance, lives for the lifetime of the app.
helix::NrfOtaFlashBackend g_otaBackend;
helix::OtaManager         g_otaManager{g_otaBackend};
} // namespace

const nrfx_twim_t g_twim0 = NRFX_TWIM_INSTANCE(0);
const nrfx_twim_t g_twim1 = NRFX_TWIM_INSTANCE(1);

bool xiao_board_init_i2c() {
    nrfx_twim_config_t twim0Cfg{};
    twim0Cfg.scl = kTwim0SclPin;
    twim0Cfg.sda = kTwim0SdaPin;
    twim0Cfg.frequency = NRF_TWIM_FREQ_400K;
    twim0Cfg.interrupt_priority = 6;
    twim0Cfg.hold_bus_uninit = false;

    nrfx_twim_config_t twim1Cfg{};
    twim1Cfg.scl = kTwim1SclPin;
    twim1Cfg.sda = kTwim1SdaPin;
    twim1Cfg.frequency = NRF_TWIM_FREQ_400K;
    twim1Cfg.interrupt_priority = 6;
    twim1Cfg.hold_bus_uninit = false;

    if (nrfx_twim_init(&g_twim0, &twim0Cfg, nullptr, nullptr) != NRFX_SUCCESS) return false;
    if (nrfx_twim_init(&g_twim1, &twim1Cfg, nullptr, nullptr) != NRFX_SUCCESS) return false;
    nrfx_twim_enable(&g_twim0);
    nrfx_twim_enable(&g_twim1);
    return true;
}

extern "C" bool __attribute__((weak)) xiao_ble_stack_notify(const uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

extern "C" uint8_t __attribute__((weak)) xiao_mocap_calibration_command() {
    return 0;
}

extern "C" bool __attribute__((weak)) xiao_mocap_sync_anchor(uint64_t* localUs, uint64_t* remoteUs) {
    (void)localUs;
    (void)remoteUs;
    return false;
}

extern "C" bool __attribute__((weak)) xiao_mocap_health_sample(
    uint16_t* batteryMv,
    uint8_t* batteryPercent,
    uint8_t* linkQuality,
    uint16_t* droppedFrames,
    uint8_t* flags) {
    (void)batteryMv;
    (void)batteryPercent;
    (void)linkQuality;
    (void)droppedFrames;
    (void)flags;
    return false;
}

extern "C" bool sf_mocap_ble_notify(const uint8_t* data, size_t len) {
    return xiao_ble_stack_notify(data, len);
}

extern "C" uint8_t sf_mocap_calibration_command() {
    return xiao_mocap_calibration_command();
}

extern "C" bool sf_mocap_sync_anchor(uint64_t* localUs, uint64_t* remoteUs) {
    return xiao_mocap_sync_anchor(localUs, remoteUs);
}

extern "C" bool sf_mocap_health_sample(
    uint16_t* batteryMv,
    uint8_t* batteryPercent,
    uint8_t* linkQuality,
    uint16_t* droppedFrames,
    uint8_t* flags) {
    return xiao_mocap_health_sample(
        batteryMv, batteryPercent, linkQuality, droppedFrames, flags);
}

// ---------------------------------------------------------------------------
// OTA hooks — BLE stack layer overrides these with strong symbols
// ---------------------------------------------------------------------------

extern "C" __attribute__((weak)) void xiao_ota_confirm_image() {
    // In MCUboot overwrite-only mode no explicit confirmation is required.
    // Swap-with-revert builds should write image_ok=1 via MCUboot's API here.
}

extern "C" __attribute__((weak))
uint8_t xiao_ota_control(uint8_t cmd, const uint8_t* payload, size_t payloadLen) {
    if (cmd == 0) return 0;  // polling no-op
    if (cmd == 1 && payloadLen >= 8) {
        uint32_t imageSize = 0;
        uint32_t expectedCrc = 0;
        for (int i = 0; i < 4; ++i) imageSize  |= static_cast<uint32_t>(payload[i]) << (i * 8);
        for (int i = 0; i < 4; ++i) expectedCrc|= static_cast<uint32_t>(payload[4 + i]) << (i * 8);
        return static_cast<uint8_t>(g_otaManager.begin(imageSize, expectedCrc));
    }
    if (cmd == 2) return static_cast<uint8_t>(g_otaManager.commit());
    if (cmd == 3) { g_otaManager.abort(); return 0; }
    return static_cast<uint8_t>(helix::OtaStatus::ERROR_INVALID_STATE);
}

extern "C" __attribute__((weak))
uint8_t xiao_ota_write_chunk(uint32_t offset, const uint8_t* data, size_t len) {
    return static_cast<uint8_t>(g_otaManager.writeChunk(offset, data, len));
}
