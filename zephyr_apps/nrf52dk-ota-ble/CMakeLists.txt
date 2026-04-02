cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

get_filename_component(HELIX_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

target_sources(app PRIVATE
  src/main.cpp
  src/ZephyrOtaFlashBackend.cpp
  ${HELIX_ROOT}/firmware/common/ota/OtaManager.cpp
  ${HELIX_ROOT}/firmware/common/ota/BleOtaService.cpp
)

target_include_directories(app PRIVATE
  src
  ${HELIX_ROOT}/firmware/common/ota
)
