cmake_minimum_required(VERSION 3.20)
project(helixdrift LANGUAGES C CXX)

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
)

if(HELIXDRIFT_BUILD_NRF_EXAMPLES)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tools/nrf/stubs/include)

    set(SENSORFUSION_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SENSORFUSION_BUILD_APP OFF CACHE BOOL "" FORCE)
    set(SENSORFUSION_PLATFORM nrf52 CACHE STRING "" FORCE)
    add_subdirectory(external/SensorFusion)

    add_executable(nrf52_blinky
        examples/nrf52-blinky/src/main.cpp
    )
    target_include_directories(nrf52_blinky PRIVATE
        tools/nrf/stubs/include
    )
    target_link_libraries(nrf52_blinky PRIVATE helix_blink)

    add_executable(nrf52_mocap_node
        examples/nrf52-mocap-node/src/main.cpp
    )
    target_include_directories(nrf52_mocap_node PRIVATE
        tools/nrf/stubs/include
        firmware/common
    )
    target_link_libraries(nrf52_mocap_node PRIVATE
        helix_mocap_common
        sensorfusion_middleware
        sensorfusion_platform_nrf52
    )
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
        tests/test_mocap_profiles.cpp
    )
    target_link_libraries(helix_tests PRIVATE
        helix_blink
        helix_mocap_common
        GTest::gtest_main
    )
    include(GoogleTest)
    gtest_discover_tests(helix_tests)
endif()
