set(SF_DRIVERS ${CMAKE_CURRENT_SOURCE_DIR}/../../../external/SensorFusion/drivers)
set(SF_MIDDLEWARE ${CMAKE_CURRENT_SOURCE_DIR}/../../../external/SensorFusion/middleware)

idf_component_register(
    SRCS
        "main.c"
        "../src/board_wroom32.cpp"
        "../src/EspI2CBus.cpp"
        "../src/SensorFusionTask.cpp"
        "../src/Esp32OtaFlashBackend.cpp"
        "../../../firmware/common/ota/OtaManager.cpp"
        "../../../firmware/common/ota/BleOtaService.cpp"
        "${SF_DRIVERS}/mpu6050/MPU6050.cpp"
        "${SF_DRIVERS}/bmm350/BMM350.cpp"
        "${SF_MIDDLEWARE}/ahrs/MahonyAHRS.cpp"
    INCLUDE_DIRS
        "."
        "../src"
        "../../../firmware/common/ota"
        "${SF_DRIVERS}/hal"
        "${SF_DRIVERS}/mpu6050"
        "${SF_DRIVERS}/bmm350"
        "${SF_MIDDLEWARE}/common"
        "${SF_MIDDLEWARE}/ahrs"
    REQUIRES
        app_update
        bt
        esp_driver_i2c
        esp_hw_support
        esp_rom
        esp_timer
        freertos
        log
        nvs_flash
)
