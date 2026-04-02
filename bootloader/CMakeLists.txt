cmake_minimum_required(VERSION 3.20)
project(helixdrift_bootloader LANGUAGES C ASM)

get_filename_component(HELIXDRIFT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

function(helix_add_bootloader_artifacts target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> $<TARGET_FILE:${target}>.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${target}> $<TARGET_FILE:${target}>.hex
        COMMENT "Generating ${target} .bin and .hex artifacts"
        VERBATIM
    )
endfunction()

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
    "${HELIXDRIFT_ROOT}/tools/nrf/baremetal/include"
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
    "${HELIXDRIFT_ROOT}/tools/nrf/baremetal/include"
)

target_compile_definitions(bootutil PUBLIC
    MCUBOOT_IMAGE_HEADER_SIZE=0x200
)
target_compile_options(bootutil PRIVATE
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

target_link_libraries(bootutil PUBLIC tinycrypt)

# ---------------------------------------------------------------------------
# Bootloader executables
# ---------------------------------------------------------------------------
set(BOOT_LD "${HELIXDRIFT_ROOT}/tools/linker/xiao_nrf52840_boot.ld")
set(DK_BOOT_LD "${HELIXDRIFT_ROOT}/tools/linker/nrf52dk_nrf52832_boot.ld")
set(NRF_STARTUP "${HELIXDRIFT_ROOT}/firmware/platform/startup_nrf52840.S")
set(DK_STARTUP "${HELIXDRIFT_ROOT}/firmware/platform/startup_nrf52dk_minimal.S")

function(helix_add_bootloader_target target startup linker)
    add_executable(${target}
        "${startup}"
        main.c
        port/flash_map_backend.c
        port/mcuboot_port.c
    )
    target_include_directories(${target} PRIVATE
        "${MCUBOOT_DIR}/boot/bootutil/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/port"
        "${MCUBOOT_SUPPORT_DIR}"
        "${MCUBOOT_MBEDTLS_ASN1_DIR}"
        "${HELIXDRIFT_ROOT}/tools/nrf/baremetal/include"
    )
    target_compile_definitions(${target} PRIVATE
        MCUBOOT_IMAGE_HEADER_SIZE=0x200
    )
    target_compile_options(${target} PRIVATE
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
    )
    target_link_libraries(${target} PRIVATE
        bootutil
        tinycrypt
    )
    target_link_options(${target} PRIVATE
        -T${linker}
        -Wl,-Map=${target}.map
        -Wl,--print-memory-usage
    )
    set_target_properties(${target} PROPERTIES LINK_DEPENDS "${linker}")
endfunction()

helix_add_bootloader_target(nrf52_bootloader "${NRF_STARTUP}" "${BOOT_LD}")
target_compile_definitions(nrf52_bootloader PRIVATE
    HELIX_FAULT_LED_PIN=26u
    HELIX_FLASH_AREA_BOOTLOADER_OFF=0x00000000u
    HELIX_FLASH_AREA_BOOTLOADER_SIZE=98304u
    HELIX_FLASH_AREA_IMAGE_0_OFF=0x00018000u
    HELIX_FLASH_AREA_IMAGE_0_SIZE=360448u
    HELIX_FLASH_AREA_IMAGE_1_OFF=0x00070000u
    HELIX_FLASH_AREA_IMAGE_1_SIZE=360448u
    HELIX_FLASH_AREA_SCRATCH_OFF=0x000C8000u
    HELIX_FLASH_AREA_SCRATCH_SIZE=32768u
)
helix_add_bootloader_artifacts(nrf52_bootloader)

helix_add_bootloader_target(nrf52dk_bootloader "${DK_STARTUP}" "${DK_BOOT_LD}")
target_compile_definitions(nrf52dk_bootloader PRIVATE
    HELIX_FAULT_LED_PIN=20u
    HELIX_BOOT_UART_TX_PIN=6u
    HELIX_BOOT_UART_RX_PIN=8u
    HELIX_FLASH_AREA_BOOTLOADER_OFF=0x00000000u
    HELIX_FLASH_AREA_BOOTLOADER_SIZE=98304u
    HELIX_FLASH_AREA_IMAGE_0_OFF=0x00018000u
    HELIX_FLASH_AREA_IMAGE_0_SIZE=147456u
    HELIX_FLASH_AREA_IMAGE_1_OFF=0x0003C000u
    HELIX_FLASH_AREA_IMAGE_1_SIZE=147456u
    HELIX_FLASH_AREA_SCRATCH_OFF=0x00060000u
    HELIX_FLASH_AREA_SCRATCH_SIZE=32768u
)
helix_add_bootloader_artifacts(nrf52dk_bootloader)
