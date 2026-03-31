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

add_library(helix_ota STATIC
    firmware/common/ota/OtaManager.cpp
    firmware/common/ota/BleOtaService.cpp
)
target_include_directories(helix_ota PUBLIC
    firmware/common/ota
)

if(HELIXDRIFT_BUILD_NRF_EXAMPLES)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tools/nrf/stubs/include)
    add_compile_definitions(NRFX_STUB)

    function(helix_add_nrf_artifacts target)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> $<TARGET_FILE:${target}>.bin
            COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${target}> $<TARGET_FILE:${target}>.hex
            COMMENT "Generating ${target} .bin and .hex artifacts"
            VERBATIM
        )
    endfunction()

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
    helix_add_nrf_artifacts(nrf52_blinky)

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
    helix_add_nrf_artifacts(nrf52_mocap_node)
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
        tests/test_ble_ota_service.cpp
    )
    target_include_directories(helix_tests PRIVATE
        tests/mocks
        firmware/common/ota
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

    add_library(helix_simulators STATIC
        simulators/i2c/VirtualI2CBus.cpp
        simulators/sensors/Lps22dfSimulator.cpp
        simulators/sensors/Lsm6dsoSimulator.cpp
        simulators/sensors/Bmm350Simulator.cpp
        simulators/storage/At24CxxSimulator.cpp
        simulators/gimbal/VirtualGimbal.cpp
        simulators/magnetic/MagneticEnvironment.cpp
        simulators/magnetic/HardIronCalibrator.cpp
        simulators/magnetic/CalibratedMagSensor.cpp
        simulators/rf/VirtualRFMedium.cpp
        simulators/rf/VirtualSyncNode.cpp
        simulators/rf/VirtualSyncMaster.cpp
    )
    target_include_directories(helix_simulators PUBLIC
        firmware/common
        simulators/i2c
        simulators/fixtures
        simulators/sensors
        simulators/storage
        simulators/gimbal
        simulators/magnetic
        simulators/rf
        external/SensorFusion/drivers/lsm6dso
        external/SensorFusion/drivers/bmm350
        external/SensorFusion/drivers/calibration
        external/SensorFusion/drivers/lps22df
        external/SensorFusion/drivers/hal
        external/SensorFusion/middleware/codec
        external/SensorFusion/middleware/motion
        external/SensorFusion/middleware/ahrs
        external/SensorFusion/middleware/common
    )

    add_library(helix_sensorfusion STATIC
        external/SensorFusion/drivers/calibration/CalibrationStore.cpp
        external/SensorFusion/drivers/lsm6dso/LSM6DSO.cpp
        external/SensorFusion/drivers/bmm350/BMM350.cpp
        external/SensorFusion/drivers/lps22df/LPS22DF.cpp
        external/SensorFusion/middleware/codec/FrameCodec.cpp
        external/SensorFusion/middleware/motion/MocapNodePipeline.cpp
        external/SensorFusion/middleware/ahrs/MahonyAHRS.cpp
    )
    target_include_directories(helix_sensorfusion PUBLIC
        external/SensorFusion/drivers/calibration
        external/SensorFusion/drivers/lsm6dso
        external/SensorFusion/drivers/bmm350
        external/SensorFusion/drivers/lps22df
        external/SensorFusion/middleware/codec
        external/SensorFusion/middleware/motion
        external/SensorFusion/middleware/ahrs
        external/SensorFusion/drivers/hal
        external/SensorFusion/middleware/common
    )

    add_executable(helix_integration_tests
        simulators/tests/test_csv_export.cpp
        simulators/tests/test_virtual_i2c_bus.cpp
        simulators/tests/test_motion_profiles.cpp
        simulators/tests/test_virtual_sensor_assembly.cpp
        simulators/tests/test_virtual_mocap_node_harness.cpp
        simulators/tests/test_sim_metrics.cpp
        simulators/tests/test_pose_drift.cpp
        simulators/tests/test_pose_axis_characterization.cpp
        simulators/tests/test_pose_gain_characterization.cpp
        simulators/tests/test_pose_joint_angle.cpp
        simulators/tests/test_pose_calibration_effectiveness.cpp
        simulators/tests/test_pose_mahony_tuning.cpp
        simulators/tests/test_pose_orientation_accuracy.cpp
        simulators/tests/test_lps22df_simulator.cpp
        simulators/tests/test_bmm350_simulator.cpp
        simulators/tests/test_lsm6dso_simulator.cpp
        simulators/tests/test_at24cxx_simulator.cpp
        simulators/tests/test_virtual_gimbal.cpp
        simulators/tests/test_magnetic_environment.cpp
        simulators/tests/test_hard_iron_calibrator.cpp
        simulators/tests/test_calibrated_mag_sensor.cpp
        simulators/tests/test_ahrs_mag_robustness.cpp
        simulators/tests/test_rf_medium_basic.cpp
        simulators/tests/test_sync_node_basic.cpp
        simulators/tests/test_rf_sync_basic.cpp
        simulators/tests/test_rf_sync_robustness.cpp
        simulators/tests/test_body_chain_sync.cpp
        simulators/tests/test_sensor_fusion_integration.cpp
    )
    target_include_directories(helix_integration_tests PRIVATE
        firmware/common
        simulators/i2c
        simulators/fixtures
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
    target_compile_definitions(helix_integration_tests PRIVATE
        HELIXDRIFT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
    include(GoogleTest)
    gtest_discover_tests(helix_integration_tests)
endif()
