cmake_minimum_required(VERSION 3.20)
project(helixdrift_bootloader LANGUAGES C ASM)

# ---------------------------------------------------------------------------
# Fetch MCUboot (bootutil core only — no Zephyr, no MyNewt)
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    mcuboot
    GIT_REPOSITORY https://github.com/mcu-tools/mcuboot.git
    GIT_TAG        v2.1.0
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(mcuboot)
if(NOT mcuboot_POPULATED)
    FetchContent_Populate(mcuboot)
endif()

set(MCUBOOT_DIR "${mcuboot_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# tinycrypt — bundled with MCUboot, provides SHA-256 and Ed25519 for signing
# ---------------------------------------------------------------------------
file(GLOB TINYCRYPT_SRCS
    "${MCUBOOT_DIR}/ext/tinycrypt/lib/source/*.c"
)

add_library(tinycrypt STATIC ${TINYCRYPT_SRCS})
target_include_directories(tinycrypt PUBLIC
    "${MCUBOOT_DIR}/ext/tinycrypt/lib/include"
)

# ---------------------------------------------------------------------------
# bootutil — the portable MCUboot core logic
# ---------------------------------------------------------------------------
set(BOOTUTIL_SRCS
    "${MCUBOOT_DIR}/boot/bootutil/src/bootutil_misc.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/bootutil_public.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/image_validate.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/image_ed25519.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/loader.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/swap_misc.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/swap_scratch.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/caps.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/fault_injection_hardening.c"
    "${MCUBOOT_DIR}/boot/bootutil/src/fault_injection_hardening_delay_rng_mbedtls.c"
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
    "${CMAKE_SOURCE_DIR}/tools/nrf/stubs/include"  # nrfx_nvmc.h (stub only)
)

target_compile_definitions(bootutil PUBLIC
    MCUBOOT_IMAGE_HEADER_SIZE=0x200
    NRFX_STUB    # enable the no-op stub; remove for real hardware builds
)

target_link_libraries(bootutil PUBLIC tinycrypt)

# ---------------------------------------------------------------------------
# Flash map backend (nRF52840 NVMC implementation)
# ---------------------------------------------------------------------------
add_library(flash_map_backend STATIC
    port/flash_map_backend.c
)
target_include_directories(flash_map_backend PUBLIC
    "${MCUBOOT_DIR}/boot/bootutil/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/port"
    "${CMAKE_SOURCE_DIR}/tools/nrf/stubs/include"
)

# ---------------------------------------------------------------------------
# Bootloader executable
# ---------------------------------------------------------------------------
set(BOOT_LD "${CMAKE_SOURCE_DIR}/tools/linker/xiao_nrf52840_boot.ld")
set(NRF_STARTUP "${CMAKE_SOURCE_DIR}/firmware/platform/startup_nrf52840.S")

add_executable(nrf52_bootloader
    "${NRF_STARTUP}"
    main.c
)

target_include_directories(nrf52_bootloader PRIVATE
    "${MCUBOOT_DIR}/boot/bootutil/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/port"
    "${CMAKE_SOURCE_DIR}/tools/nrf/stubs/include"
)

target_compile_definitions(nrf52_bootloader PRIVATE
    MCUBOOT_IMAGE_HEADER_SIZE=0x200
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
