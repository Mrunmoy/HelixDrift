cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(helix_nrf52840_mocap_bridge)

get_filename_component(HELIX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

target_sources(app PRIVATE
  src/usbd_init.c
  src/main.cpp
  src/ota_hub_relay.cpp
  ${HELIX_ROOT}/firmware/common/ota/BleOtaService.cpp
)

# OTA flash backend and manager only needed for Tags (node role with MCUboot)
if(CONFIG_HELIX_MOCAP_BRIDGE_ROLE_NODE)
  target_sources(app PRIVATE
    src/ZephyrOtaFlashBackend.cpp
    ${HELIX_ROOT}/firmware/common/ota/OtaManager.cpp
  )
endif()

target_include_directories(app PRIVATE
  src
  ${HELIX_ROOT}/firmware/common/ota
)
