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

add_library(helix_ota STATIC
    firmware/common/ota/OtaManager.cpp
    firmware/common/ota/BleOtaService.cpp
)
target_include_directories(helix_ota PUBLIC
    firmware/common/ota
)

# Simulator library
add_library(helix_simulators STATIC
    simulators/i2c/VirtualI2CBus.cpp
    simulators/sensors/Lps22dfSimulator.cpp
    simulators/sensors/Lsm6dsoSimulator.cpp
    simulators/sensors/Bmm350Simulator.cpp
    simulators/storage/At24CxxSimulator.cpp
    simulators/gimbal/VirtualGimbal.cpp
)
target_include_directories(helix_simulators PUBLIC
    simulators/i2c
    simulators/sensors
    simulators/storage
    simulators/gimbal
    external/SensorFusion/drivers/hal
    external/SensorFusion/middleware/common
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

    # Unit tests (existing)
    add_executable(helix_tests
        tests/test_blink_engine.cpp
        tests/test_mocap_ble_sender.cpp
        tests/test_mocap_health_telemetry.cpp
        tests/test_mocap_node_loop.cpp
        tests/test_timestamp_synchronized_transport.cpp
        tests/test_mocap_profiles.cpp
        tests/test_ota_manager.cpp
        tests/test_esp32_ota_flash_backend.cpp
        tests/test_ble_ota_service.cpp
        examples/esp32s3-mocap-node/src/Esp32OtaFlashBackend.cpp
    )
    target_include_directories(helix_tests PRIVATE
        tests/mocks
        firmware/common/ota
        tools/esp/stubs/include
        examples/esp32s3-mocap-node/src
    )
    target_compile_definitions(helix_tests PRIVATE ESP32_STUB)
    target_link_libraries(helix_tests PRIVATE
        helix_blink
        helix_mocap_common
        helix_ota
        GTest::gtest_main
        GTest::gmock
    )
    include(GoogleTest)
    gtest_discover_tests(helix_tests)

    # SensorFusion drivers library (for integration tests)
    add_library(helix_sensorfusion STATIC
        external/SensorFusion/drivers/lsm6dso/LSM6DSO.cpp
        external/SensorFusion/drivers/bmm350/BMM350.cpp
        external/SensorFusion/drivers/lps22df/LPS22DF.cpp
        external/SensorFusion/middleware/motion/MocapNodePipeline.cpp
        external/SensorFusion/middleware/ahrs/MahonyAHRS.cpp
    )
    target_include_directories(helix_sensorfusion PUBLIC
        external/SensorFusion/drivers/lsm6dso
        external/SensorFusion/drivers/bmm350
        external/SensorFusion/drivers/lps22df
        external/SensorFusion/middleware/motion
        external/SensorFusion/middleware/ahrs
        external/SensorFusion/drivers/hal
        external/SensorFusion/middleware/common
    )

    # Integration tests (simulators)
    add_executable(helix_integration_tests
        simulators/tests/test_virtual_i2c_bus.cpp
        simulators/tests/test_lps22df_simulator.cpp
        simulators/tests/test_bmm350_simulator.cpp
        simulators/tests/test_lsm6dso_simulator.cpp
        simulators/tests/test_at24cxx_simulator.cpp
        simulators/tests/test_virtual_gimbal.cpp
        simulators/tests/test_sensor_fusion_integration.cpp
    )
    target_include_directories(helix_integration_tests PRIVATE
        simulators/i2c
        simulators/sensors
        simulators/storage
        simulators/gimbal
        external/SensorFusion/drivers/hal
        external/SensorFusion/middleware/common
    )
    target_link_libraries(helix_integration_tests PRIVATE
        helix_simulators
        helix_sensorfusion
        GTest::gtest_main
        GTest::gmock
    )
    include(GoogleTest)
    gtest_discover_tests(helix_integration_tests)
endif()
