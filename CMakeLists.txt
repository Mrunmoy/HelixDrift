cmake_minimum_required(VERSION 3.20)
project(helixdrift LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HELIXDRIFT_BUILD_TESTS "Build host tests" ON)
option(HELIXDRIFT_BUILD_NRF_EXAMPLES "Build nRF firmware examples" ON)

add_library(helix_blink STATIC
    firmware/common/BlinkEngine.cpp
)
target_include_directories(helix_blink PUBLIC
    firmware/common
)

add_library(helix_mocap_common STATIC
    firmware/common/MocapBleSender.cpp
)
target_include_directories(helix_mocap_common PUBLIC
    firmware/common
    external/SensorFusion/middleware/codec
    external/SensorFusion/middleware/common
    external/SensorFusion/drivers/hal
)

# OTA manager — platform-independent, testable on host
add_library(helix_ota STATIC
    firmware/common/ota/OtaManager.cpp
)
target_include_directories(helix_ota PUBLIC
    firmware/common/ota
)

if(HELIXDRIFT_BUILD_NRF_EXAMPLES)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tools/nrf/stubs/include)

    set(SENSORFUSION_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SENSORFUSION_BUILD_APP OFF CACHE BOOL "" FORCE)
    set(SENSORFUSION_PLATFORM nrf52 CACHE STRING "" FORCE)
    add_subdirectory(external/SensorFusion)

    set(NRF_APP_LD "${CMAKE_CURRENT_SOURCE_DIR}/tools/linker/xiao_nrf52840_app.ld")
    set(NRF_STARTUP "${CMAKE_CURRENT_SOURCE_DIR}/firmware/platform/startup_nrf52840.S")

    add_executable(nrf52_blinky
        ${NRF_STARTUP}
        examples/nrf52-blinky/src/main.cpp
    )
    target_include_directories(nrf52_blinky PRIVATE
        tools/nrf/stubs/include
    )
    target_link_libraries(nrf52_blinky PRIVATE helix_blink)
    target_link_options(nrf52_blinky PRIVATE -T${NRF_APP_LD} -Wl,-Map=nrf52_blinky.map)
    set_target_properties(nrf52_blinky PROPERTIES LINK_DEPENDS "${NRF_APP_LD}")

    add_executable(nrf52_mocap_node
        ${NRF_STARTUP}
        examples/nrf52-mocap-node/src/main.cpp
        examples/nrf52-mocap-node/src/board_xiao_nrf52840.cpp
        examples/nrf52-mocap-node/src/NrfOtaFlashBackend.cpp
    )
    target_include_directories(nrf52_mocap_node PRIVATE
        tools/nrf/stubs/include
        firmware/common
        firmware/common/ota
    )
    target_link_libraries(nrf52_mocap_node PRIVATE
        helix_mocap_common
        helix_ota
        sensorfusion_middleware
        sensorfusion_platform_nrf52
    )
    target_link_options(nrf52_mocap_node PRIVATE -T${NRF_APP_LD} -Wl,-Map=nrf52_mocap_node.map)
    set_target_properties(nrf52_mocap_node PROPERTIES LINK_DEPENDS "${NRF_APP_LD}")
endif()

if(HELIXDRIFT_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(helix_tests
        tests/test_blink_engine.cpp
        tests/test_mocap_ble_sender.cpp
        tests/test_mocap_health_telemetry.cpp
        tests/test_mocap_node_loop.cpp
        tests/test_timestamp_synchronized_transport.cpp
        tests/test_mocap_profiles.cpp
        tests/test_ota_manager.cpp
    )
    target_include_directories(helix_tests PRIVATE
        firmware/common/ota
        tests/mocks
    )
    target_link_libraries(helix_tests PRIVATE
        helix_blink
        helix_mocap_common
        helix_ota
        GTest::gtest_main
        GTest::gmock
    )
    include(GoogleTest)
    gtest_discover_tests(helix_tests)
endif()
