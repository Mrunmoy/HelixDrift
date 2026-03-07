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

if(HELIXDRIFT_BUILD_NRF_EXAMPLES)
    add_executable(nrf52_blinky
        examples/nrf52-blinky/src/main.cpp
    )
    target_include_directories(nrf52_blinky PRIVATE
        tools/nrf/stubs/include
    )
    target_link_libraries(nrf52_blinky PRIVATE helix_blink)
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
    )
    target_link_libraries(helix_tests PRIVATE
        helix_blink
        GTest::gtest_main
    )
    include(GoogleTest)
    gtest_discover_tests(helix_tests)
endif()
