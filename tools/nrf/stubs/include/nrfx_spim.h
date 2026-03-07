#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct { int dummy; } nrfx_spim_t;
#ifndef NRFX_ERR_T_DEFINED
#define NRFX_ERR_T_DEFINED
typedef enum { NRFX_SUCCESS = 0 } nrfx_err_t;
#endif
typedef struct {
    const uint8_t* p_tx_buffer;
    size_t tx_length;
    uint8_t* p_rx_buffer;
    size_t rx_length;
} nrfx_spim_xfer_desc_t;
#define NRFX_SPIM_XFER_TRX(tx, tx_len, rx, rx_len) (nrfx_spim_xfer_desc_t{(tx),(tx_len),(rx),(rx_len)})
#define NRFX_SPIM_XFER_TX(tx, tx_len) (nrfx_spim_xfer_desc_t{(tx),(tx_len),nullptr,0})
static inline nrfx_err_t nrfx_spim_xfer(const nrfx_spim_t*, const nrfx_spim_xfer_desc_t*, uint32_t) { return NRFX_SUCCESS; }
