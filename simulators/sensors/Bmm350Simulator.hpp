#pragma once

#include "VirtualI2CBus.hpp"
#include "Quaternion.hpp"
#include "Vec3.hpp"
#include <cstdint>
#include <random>

namespace sim {

/**
 * @brief BMM350 Magnetometer Simulator
 * 
 * Simulates the BMM350 3-axis magnetometer with:
 * - Register map (CHIP_ID, PMU, OTP, data registers)
 * - Mag data generation from orientation and earth field
 * - Hard/soft iron error injection
 * - OTP calibration data simulation
 * - Temperature sensor
 */
class Bmm350Simulator : public I2CDevice {
public:
    // BMM350 Register addresses
    static constexpr uint8_t REG_CHIP_ID         = 0x00;
    static constexpr uint8_t REG_PMU_CMD_AGGR    = 0x04;
    static constexpr uint8_t REG_PMU_CMD_AXIS_EN = 0x05;
    static constexpr uint8_t REG_PMU_CMD         = 0x06;
    static constexpr uint8_t REG_PMU_CMD_STATUS  = 0x07;
    static constexpr uint8_t REG_INT_CTRL        = 0x2E;
    static constexpr uint8_t REG_INT_STATUS      = 0x30;
    static constexpr uint8_t REG_MAG_X_XLSB      = 0x31;
    static constexpr uint8_t REG_TEMP_XLSB       = 0x3A;
    static constexpr uint8_t REG_OTP_CMD         = 0x50;
    static constexpr uint8_t REG_OTP_DATA_MSB    = 0x52;
    static constexpr uint8_t REG_OTP_DATA_LSB    = 0x53;
    static constexpr uint8_t REG_OTP_STATUS      = 0x55;
    static constexpr uint8_t REG_CMD             = 0x7E;

    static constexpr uint8_t CHIP_ID_VALUE = 0x33;
    static constexpr uint8_t DEFAULT_ADDRESS = 0x14;

    // Default earth magnetic field (µT)
    static constexpr float DEFAULT_EARTH_FIELD_HORIZONTAL = 25.0f;
    static constexpr float DEFAULT_EARTH_FIELD_VERTICAL   = 40.0f;

    // Conversion constants (from driver)
    static constexpr float UT_SCALE_XY = 14.55f;
    static constexpr float UT_SCALE_Z  = 9.0f;
    static constexpr float TEMP_SCALE  = 0.00204f;
    static constexpr float TEMP_OFFSET = -25.49f;

    struct ErrorConfig {
        sf::Vec3 hardIron{0.0f, 0.0f, 0.0f};     // Constant offset (µT)
        sf::Vec3 softIronScale{1.0f, 1.0f, 1.0f}; // Axis scaling
        sf::Vec3 bias{0.0f, 0.0f, 0.0f};          // Additional bias
        float noiseStdDev = 0.0f;                 // Gaussian noise std dev (µT)
    };

    Bmm350Simulator();

    // I2CDevice interface
    bool readRegister(uint8_t reg, uint8_t* buf, size_t len) override;
    bool writeRegister(uint8_t reg, const uint8_t* data, size_t len) override;
    bool probe() override;

    // Ground truth setting
    void setOrientation(const sf::Quaternion& q);
    void setEarthField(const sf::Vec3& field);
    void setTemperature(float tempC);
    void setSeed(uint32_t seed);

    // Error injection
    void setErrors(const ErrorConfig& errors);
    const ErrorConfig& getErrors() const { return errors_; }

    // OTP data configuration
    void setOtpData(uint8_t wordAddr, uint16_t data);
    uint16_t getOtpData(uint8_t wordAddr) const;

    // Raw sensor data access (for testing)
    sf::Vec3 getRawMagData() const;  // Returns mag field in µT
    float getRawTemperature() const;

    // Get currently generated values (after all transformations)
    sf::Vec3 getMagData() const;

private:
    // Register storage
    uint8_t registers_[256] = {};
    
    // OTP storage (32 words of 16 bits each)
    uint16_t otpData_[32] = {};
    uint8_t lastOtpCmd_ = 0;

    // Ground truth state
    sf::Quaternion orientation_{1.0f, 0.0f, 0.0f, 0.0f};
    sf::Vec3 earthField_{DEFAULT_EARTH_FIELD_HORIZONTAL, 0.0f, DEFAULT_EARTH_FIELD_VERTICAL};
    float temperature_ = 25.0f;

    // Error injection
    ErrorConfig errors_;
    mutable std::mt19937 rng_;
    mutable std::normal_distribution<float> noiseDist_{0.0f, 1.0f};

    // Internal methods
    void updateMagData();
    void updateTemperatureData();
    uint32_t encodeMagToRaw(float valueUT, float scale);
    uint32_t encodeTempToRaw(float tempC);
    float generateNoise() const;
    
    // OTP handling
    void handleOtpCommand(uint8_t cmd);
    void updateOtpDataRegisters();
};

} // namespace sim
