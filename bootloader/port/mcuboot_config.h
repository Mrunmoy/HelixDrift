/* mcuboot_config.h
 *
 * MCUboot feature configuration for the XIAO nRF52840 standalone bootloader.
 *
 * Upgrade strategy: MCUBOOT_OVERWRITE_ONLY
 *   - MCUboot overwrites the primary slot with the secondary slot when a valid
 *     newer image is found.  No swap/scratch area required.
 *   - No revert support; for rollback use version pinning in CI.
 *
 * Signing: Ed25519 (ECDSA over Curve25519)
 *   - imgtool.py generates Ed25519 keys; the public key is embedded here.
 *   - Uses the tinycrypt Ed25519 implementation bundled with MCUboot.
 *
 * Image validation: SHA-256 hash embedded in the signed image header.
 */

#pragma once

/* ---- Upgrade strategy --------------------------------------------------- */
#define MCUBOOT_OVERWRITE_ONLY        1

/* ---- Signing algorithm -------------------------------------------------- */
#define MCUBOOT_SIGN_ED25519          1

/* ---- Hash algorithm ------------------------------------------------------ */
#define MCUBOOT_USE_TINYCRYPT         1

/* ---- Image header size (bytes) ------------------------------------------ */
/* Must match the --header-size argument passed to imgtool.py. */
#define MCUBOOT_MAX_IMG_SECTORS       256
#define MCUBOOT_IMAGE_HEADER_SIZE     0x200   /* 512 bytes */

/* ---- Flash write alignment (bytes) -------------------------------------- */
/* nRF52840 NVMC requires 4-byte aligned writes. */
#define MCUBOOT_BOOT_MAX_ALIGN        4

/* ---- Image topology ------------------------------------------------------ */
#define MCUBOOT_IMAGE_NUMBER          1

/* ---- Bare-metal stubs expected by MCUboot ------------------------------- */
#define MCUBOOT_WATCHDOG_FEED()       do { } while (0)
#define MCUBOOT_CPU_IDLE()            do { } while (0)

/* ---- Assert -------------------------------------------------------------- */
/* Trap on assertion failure — loop forever. */
#define MCUBOOT_ASSERT_AS_TRAP        1

/* ---- Primary / secondary slot sizes (must match linker layout) ----------- */
/* These values are passed as -D flags from CMakeLists; define defaults here. */
#ifndef FLASH_AREA_IMAGE_SECTOR_SIZE
#define FLASH_AREA_IMAGE_SECTOR_SIZE  4096u     /* nRF52840 page size */
#endif
