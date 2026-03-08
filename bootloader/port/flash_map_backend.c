/* flash_map_backend.c
 *
 * MCUboot flash map backend for nRF52840.
 *
 * Implements the flash_map_backend.h interface required by MCUboot's bootutil
 * core (boot/bootutil/include/flash_map_backend/flash_map_backend.h).
 *
 * Flash layout matching tools/linker/xiao_nrf52840_app.ld:
 *   FLASH_AREA_BOOTLOADER (id=0):  0x00000000, 64 KB
 *   FLASH_AREA_IMAGE_0   (id=1):  0x00010000, 384 KB  (primary slot)
 *   FLASH_AREA_IMAGE_1   (id=2):  0x00070000, 384 KB  (secondary slot)
 *   FLASH_AREA_IMAGE_SCRATCH (id=3): 0x000D0000, 32 KB  (unused in OVERWRITE_ONLY)
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "flash_map_backend/flash_map_backend.h"
#include "nrfx_nvmc.h"

/* ---- Flash area descriptor table ---------------------------------------- */

#define FLASH_AREA_ID_BOOTLOADER  0
#define FLASH_AREA_ID_IMAGE_0     1
#define FLASH_AREA_ID_IMAGE_1     2
#define FLASH_AREA_ID_SCRATCH     3

static const struct flash_area s_flash_areas[] = {
    [FLASH_AREA_ID_BOOTLOADER] = {
        .fa_id      = FLASH_AREA_ID_BOOTLOADER,
        .fa_device_id = 0,
        .pad16      = 0,
        .fa_off     = 0x00000000u,
        .fa_size    = 64u * 1024u,
    },
    [FLASH_AREA_ID_IMAGE_0] = {
        .fa_id      = FLASH_AREA_ID_IMAGE_0,
        .fa_device_id = 0,
        .pad16      = 0,
        .fa_off     = 0x00010000u,
        .fa_size    = 384u * 1024u,
    },
    [FLASH_AREA_ID_IMAGE_1] = {
        .fa_id      = FLASH_AREA_ID_IMAGE_1,
        .fa_device_id = 0,
        .pad16      = 0,
        .fa_off     = 0x00070000u,
        .fa_size    = 384u * 1024u,
    },
    [FLASH_AREA_ID_SCRATCH] = {
        .fa_id      = FLASH_AREA_ID_SCRATCH,
        .fa_device_id = 0,
        .pad16      = 0,
        .fa_off     = 0x000D0000u,
        .fa_size    = 32u * 1024u,
    },
};

#define NUM_FLASH_AREAS  (sizeof(s_flash_areas) / sizeof(s_flash_areas[0]))

/* ---- flash_map_backend API ---------------------------------------------- */

int flash_area_open(uint8_t id, const struct flash_area** fap) {
    for (size_t i = 0; i < NUM_FLASH_AREAS; ++i) {
        if (s_flash_areas[i].fa_id == id) {
            *fap = &s_flash_areas[i];
            return 0;
        }
    }
    return -1;
}

void flash_area_close(const struct flash_area* fap) {
    (void)fap;
}

int flash_area_read(const struct flash_area* fap, uint32_t off,
                    void* dst, uint32_t len) {
    if (off + len > fap->fa_size) return -1;
    memcpy(dst, (const void*)(uintptr_t)(fap->fa_off + off), len);
    return 0;
}

int flash_area_write(const struct flash_area* fap, uint32_t off,
                     const void* src, uint32_t len) {
    if (off + len > fap->fa_size) return -1;

    const uint8_t*  data = (const uint8_t*)src;
    uint32_t        addr = fap->fa_off + off;
    size_t          written = 0;

    while (written + 4u <= len) {
        uint32_t w;
        memcpy(&w, data + written, 4);
        nrfx_nvmc_word_write(addr + (uint32_t)written, w);
        while (!nrfx_nvmc_write_done_check()) {}
        written += 4u;
    }
    if (written < len) {
        /* Pad trailing bytes with 0xFF (erased value). */
        uint8_t word[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        for (size_t i = 0; i < len - written; ++i) {
            word[i] = data[written + i];
        }
        uint32_t w;
        memcpy(&w, word, 4);
        nrfx_nvmc_word_write(addr + (uint32_t)written, w);
        while (!nrfx_nvmc_write_done_check()) {}
    }
    return 0;
}

int flash_area_erase(const struct flash_area* fap, uint32_t off, uint32_t len) {
    if (off + len > fap->fa_size) return -1;
    const uint32_t page_size = NRFX_NVMC_FLASH_PAGE_SIZE;
    for (uint32_t p = 0; p < len; p += page_size) {
        nrfx_nvmc_page_erase(fap->fa_off + off + p);
        while (!nrfx_nvmc_write_done_check()) {}
    }
    return 0;
}

uint32_t flash_area_align(const struct flash_area* fap) {
    (void)fap;
    return 4u;  /* nRF52840: 4-byte write alignment */
}

uint8_t flash_area_erased_val(const struct flash_area* fap) {
    (void)fap;
    return 0xFF;
}

int flash_area_get_sectors(int fa_id, uint32_t* count,
                           struct flash_sector* sectors) {
    const struct flash_area* fap = NULL;
    if (flash_area_open((uint8_t)fa_id, &fap) != 0) return -1;

    const uint32_t page_size = NRFX_NVMC_FLASH_PAGE_SIZE;
    const uint32_t n = fap->fa_size / page_size;
    for (uint32_t i = 0; i < n; ++i) {
        sectors[i].fs_off  = i * page_size;
        sectors[i].fs_size = page_size;
    }
    *count = n;
    flash_area_close(fap);
    return 0;
}

int flash_area_id_from_image_slot(int slot) {
    return (slot == 0) ? FLASH_AREA_ID_IMAGE_0 : FLASH_AREA_ID_IMAGE_1;
}

int flash_area_id_to_image_slot(int area_id) {
    if (area_id == FLASH_AREA_ID_IMAGE_0) return 0;
    if (area_id == FLASH_AREA_ID_IMAGE_1) return 1;
    return -1;
}
