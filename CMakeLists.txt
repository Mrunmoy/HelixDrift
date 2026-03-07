cmake_minimum_required(VERSION 3.20)
project(helixdrift LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HELIXDRIFT_BUILD_TESTS "Build host tests" ON)

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
    )
    target_link_libraries(helix_tests PRIVATE
        helix_blink
        helix_mocap_common
        GTest::gtest_main
    )
    include(GoogleTest)
    gtest_discover_tests(helix_tests)
endif()
