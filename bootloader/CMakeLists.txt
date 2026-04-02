cmake_minimum_required(VERSION 3.20)
project(helixdrift_bootloader LANGUAGES C ASM)

get_filename_component(HELIXDRIFT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# ---------------------------------------------------------------------------
# Vendored MCUboot (bootutil core only — no Zephyr, no MyNewt)
# ---------------------------------------------------------------------------
set(MCUBOOT_DIR "${HELIXDRIFT_ROOT}/third_party/mcuboot")
set(MCUBOOT_SUPPORT_DIR "${MCUBOOT_DIR}/sim/mcuboot-sys/csupport")
set(MCUBOOT_MBEDTLS_ASN1_DIR "${MCUBOOT_DIR}/ext/mbedtls-asn1/include")
if(NOT EXISTS "${MCUBOOT_DIR}/boot/bootutil/include/bootutil/bootutil.h")
    message(FATAL_ERROR
        "MCUboot submodule missing. Run 'git submodule update --init --recursive'.")
endif()

# ---------------------------------------------------------------------------
# tinycrypt — bundled with MCUboot, provides SHA-256 and Ed25519 for signing
# ---------------------------------------------------------------------------
set(TINYCRYPT_SRCS
    "${MCUBOOT_DIR}/ext/tinycrypt/lib/source/utils.c"
    "${MCUBOOT_DIR}/ext/tinycrypt/lib/source/sha256.c"
)
set(TINYCRYPT_SHA512_SRCS
    "${MCUBOOT_DIR}/ext/tinycrypt-sha512/lib/source/sha512.c"
)
set(FIAT_ED25519_SRCS
    "${MCUBOOT_DIR}/ext/fiat/src/curve25519.c"
)
set(MBEDTLS_ASN1_SRCS
    "${MCUBOOT_DIR}/ext/mbedtls-asn1/src/asn1parse.c"
    "${MCUBOOT_DIR}/ext/mbedtls-asn1/src/platform_util.c"
)

add_library(tinycrypt STATIC ${TINYCRYPT_SRCS} ${TINYCRYPT_SHA512_SRCS} ${FIAT_ED25519_SRCS} ${MBEDTLS_ASN1_SRCS})
target_include_directories(tinycrypt PUBLIC
    "${MCUBOOT_DIR}/ext/tinycrypt/lib/include"
    "${MCUBOOT_DIR}/ext/tinycrypt-sha512/lib/include"
    "${MCUBOOT_DIR}/ext/fiat/src"
    "${MCUBOOT_MBEDTLS_ASN1_DIR}"
    "${MCUBOOT_DIR}/boot/bootutil/include"
    "${MCUBOOT_SUPPORT_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/port"
    "${HELIXDRIFT_ROOT}/tools/nrf/stubs/include"
)
target_compile_options(tinycrypt PRIVATE
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

# ---------------------------------------------------------------------------
# bootutil — the portable MCUboot core logic
# ---------------------------------------------------------------------------
set(BOOTUTIL_SRCS
    "${MCUBOOT_DIR}/boot/bootutil/src/bootutil_misc.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/bootutil_public.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/bootutil_find_key.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/bootutil_area.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/bootutil_img_hash.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/bootutil_loader.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/image_validate.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/image_ed25519.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/loader.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/swap_misc.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/swap_scratch.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/caps.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/fault_injection_hardening.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/fault_injection_hardening_delay_rng_mbedtls.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/tlv.c"
)
# Filter to files that exist in this MCUboot version
set(BOOTUTIL_SRCS_FILTERED "")
foreach(f IN LISTS BOOTUTIL_SRCS)
    if(EXISTS "${f}")
        list(APPEND BOOTUTIL_SRCS_FILTERED "${f}")
    endif()
endforeach()

add_library(bootutil STATIC ${BOOTUTIL_SRCS_FILTERED})

target_include_directories(bootutil PUBLIC
    "${MCUBOOT_DIR}/boot/bootutil/include"
    "${MCUBOOT_DIR}/boot/bootutil/src"         # internal headers
    "${CMAKE_CURRENT_SOURCE_DIR}/port"          # mcuboot_config.h
    "${MCUBOOT_SUPPORT_DIR}"                    # generic sysflash/flash_map stubs
    "${MCUBOOT_MBEDTLS_ASN1_DIR}"               # ASN.1 parsing for Ed25519 keys
    "${HELIXDRIFT_ROOT}/tools/nrf/stubs/include"  # nrfx_nvmc.h (stub only)
)

target_compile_definitions(bootutil PUBLIC
    MCUBOOT_IMAGE_HEADER_SIZE=0x200
    NRFX_STUB    # enable the no-op stub; remove for real hardware builds
)
target_compile_options(bootutil PRIVATE
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

target_link_libraries(bootutil PUBLIC tinycrypt)

# ---------------------------------------------------------------------------
# Flash map backend (nRF52840 NVMC implementation)
# ---------------------------------------------------------------------------
add_library(flash_map_backend STATIC
    port/flash_map_backend.c
    port/mcuboot_port.c
)
target_include_directories(flash_map_backend PUBLIC
    "${MCUBOOT_DIR}/boot/bootutil/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/port"
    "${MCUBOOT_SUPPORT_DIR}"
    "${HELIXDRIFT_ROOT}/tools/nrf/stubs/include"
)
target_compile_definitions(flash_map_backend PUBLIC NRFX_STUB)
target_compile_options(flash_map_backend PRIVATE
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

# ---------------------------------------------------------------------------
# Bootloader executable
# ---------------------------------------------------------------------------
set(BOOT_LD "${HELIXDRIFT_ROOT}/tools/linker/xiao_nrf52840_boot.ld")
set(NRF_STARTUP "${HELIXDRIFT_ROOT}/firmware/platform/startup_nrf52840.S")

add_executable(nrf52_bootloader
    "${NRF_STARTUP}"
    main.c
)

target_include_directories(nrf52_bootloader PRIVATE
    "${MCUBOOT_DIR}/boot/bootutil/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/port"
    "${MCUBOOT_SUPPORT_DIR}"
    "${MCUBOOT_MBEDTLS_ASN1_DIR}"
    "${HELIXDRIFT_ROOT}/tools/nrf/stubs/include"
)

target_compile_definitions(nrf52_bootloader PRIVATE
    MCUBOOT_IMAGE_HEADER_SIZE=0x200
    NRFX_STUB
)
target_compile_options(nrf52_bootloader PRIVATE
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

target_link_libraries(nrf52_bootloader PRIVATE
    bootutil
    flash_map_backend
    tinycrypt
)

target_link_options(nrf52_bootloader PRIVATE
    -T${BOOT_LD}
    -Wl,-Map=nrf52_bootloader.map
    -Wl,--print-memory-usage
)

set_target_properties(nrf52_bootloader PROPERTIES LINK_DEPENDS "${BOOT_LD}")
