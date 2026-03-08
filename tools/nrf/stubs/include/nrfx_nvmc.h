/* nrfx_nvmc.h — minimal stub for off-target compile validation.
 *
 * Only active when NRFX_STUB is defined (set by the CMake host/CI build).
 * Hardware builds must provide the real nrfx_nvmc driver instead and must
 * NOT define NRFX_STUB.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef NRFX_STUB

#ifdef __cplusplus
extern "C" {
#endif

/** Flash page size for nRF52840: 4 KB */
#define NRFX_NVMC_FLASH_PAGE_SIZE  4096u

/** Erase a single 4 KB flash page. @param page_addr Page-aligned address. */
static inline void nrfx_nvmc_page_erase(uint32_t page_addr) { (void)page_addr; }

/**
 * Write an aligned word (4 bytes) to flash.
 * The target location must have been erased to 0xFF first.
 */
static inline void nrfx_nvmc_word_write(uint32_t address, uint32_t value) {
    (void)address; (void)value;
}

/** Returns true when the NVMC is not busy with a previous erase/write. */
static inline bool nrfx_nvmc_write_done_check(void) { return true; }

#ifdef __cplusplus
}
#endif

#else
/* On real hardware, include the nRF5 SDK nrfx driver header. */
#error "nrfx_nvmc.h stub included without NRFX_STUB — provide the real nrfx_nvmc driver."
#endif /* NRFX_STUB */
