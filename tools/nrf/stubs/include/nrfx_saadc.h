#pragma once
#include <stdint.h>
#ifndef NRFX_ERR_T_DEFINED
#define NRFX_ERR_T_DEFINED
typedef enum { NRFX_SUCCESS = 0 } nrfx_err_t;
#endif
typedef int16_t nrf_saadc_value_t;
typedef uint8_t nrf_saadc_input_t;
typedef struct { nrf_saadc_input_t pin_p; } nrfx_saadc_channel_t;
static inline nrfx_err_t nrfx_saadc_sample_convert(uint8_t, nrf_saadc_value_t* out) { if (out) *out = 0; return NRFX_SUCCESS; }
