#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct { int dummy; } nrfx_twim_t;
#ifndef NRFX_ERR_T_DEFINED
#define NRFX_ERR_T_DEFINED
typedef enum { NRFX_SUCCESS = 0 } nrfx_err_t;
#endif
static inline nrfx_err_t nrfx_twim_tx(const nrfx_twim_t*, uint8_t, const uint8_t*, size_t, bool) { return NRFX_SUCCESS; }
static inline nrfx_err_t nrfx_twim_rx(const nrfx_twim_t*, uint8_t, uint8_t*, size_t) { return NRFX_SUCCESS; }
