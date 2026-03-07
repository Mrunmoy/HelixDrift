#pragma once

#include "NrfSdk.hpp"
#include <cstdint>

extern const nrfx_twim_t g_twim0;
extern const nrfx_twim_t g_twim1;

bool xiao_board_init_i2c();

extern "C" bool sf_mocap_ble_notify(const uint8_t* data, size_t len);
extern "C" uint8_t sf_mocap_calibration_command();
extern "C" bool sf_mocap_sync_anchor(uint64_t* localUs, uint64_t* remoteUs);
extern "C" bool sf_mocap_health_sample(
    uint16_t* batteryMv,
    uint8_t* batteryPercent,
    uint8_t* linkQuality,
    uint16_t* droppedFrames,
    uint8_t* flags);
