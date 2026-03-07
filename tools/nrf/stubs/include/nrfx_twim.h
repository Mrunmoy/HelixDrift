#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct { int instance_id; } nrfx_twim_t;
#define NRFX_TWIM_INSTANCE(id) ((nrfx_twim_t){ .instance_id = (id) })
#ifndef NRFX_ERR_T_DEFINED
#define NRFX_ERR_T_DEFINED
typedef enum { NRFX_SUCCESS = 0 } nrfx_err_t;
#endif
typedef enum { NRF_TWIM_FREQ_100K = 100000, NRF_TWIM_FREQ_400K = 400000 } nrf_twim_frequency_t;
typedef struct {
    uint32_t scl;
    uint32_t sda;
    nrf_twim_frequency_t frequency;
    uint8_t interrupt_priority;
    bool hold_bus_uninit;
} nrfx_twim_config_t;
typedef void (*nrfx_twim_evt_handler_t)(void*, void*);
static inline nrfx_err_t nrfx_twim_init(
    const nrfx_twim_t*,
    const nrfx_twim_config_t*,
    nrfx_twim_evt_handler_t,
    void*) { return NRFX_SUCCESS; }
static inline void nrfx_twim_enable(const nrfx_twim_t*) {}
static inline nrfx_err_t nrfx_twim_tx(const nrfx_twim_t*, uint8_t, const uint8_t*, size_t, bool) { return NRFX_SUCCESS; }
static inline nrfx_err_t nrfx_twim_rx(const nrfx_twim_t*, uint8_t, uint8_t*, size_t) { return NRFX_SUCCESS; }
