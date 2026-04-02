#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NRF_NVMC_FLASH_PAGE_SIZE 4096u

static inline volatile uint32_t* nrf_nvmc_reg(uintptr_t addr) {
    return (volatile uint32_t*)addr;
}

static inline bool nrf_nvmc_write_done_check(void) {
    return *nrf_nvmc_reg(0x4001E400u) != 0u;
}

static inline void nrf_nvmc_wait_ready(void) {
    while (!nrf_nvmc_write_done_check()) {
    }
}

static inline void nrf_nvmc_set_mode(uint32_t mode) {
    *nrf_nvmc_reg(0x4001E504u) = mode;
    nrf_nvmc_wait_ready();
}

static inline void nrf_nvmc_page_erase(uint32_t page_addr) {
    nrf_nvmc_set_mode(2u);
    *nrf_nvmc_reg(0x4001E508u) = page_addr;
    nrf_nvmc_wait_ready();
    nrf_nvmc_set_mode(0u);
}

static inline void nrf_nvmc_begin_write(void) {
    nrf_nvmc_set_mode(1u);
}

static inline void nrf_nvmc_end_write(void) {
    nrf_nvmc_set_mode(0u);
}

static inline void nrf_nvmc_word_write_raw(uint32_t address, uint32_t value) {
    *(volatile uint32_t*)address = value;
    nrf_nvmc_wait_ready();
}

static inline void nrf_nvmc_word_write(uint32_t address, uint32_t value) {
    nrf_nvmc_begin_write();
    nrf_nvmc_word_write_raw(address, value);
    nrf_nvmc_end_write();
}

#ifdef __cplusplus
}
#endif
