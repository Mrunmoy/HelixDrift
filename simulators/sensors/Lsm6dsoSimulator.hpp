#pragma once

#include "VirtualI2CBus.hpp"
#include "Quaternion.hpp"
#include "Vec3.hpp"
#include <cstdint>
#include <random>

namespace sim {

/**
 * @brief LSM6DSO IMU simulator
 * 
 * Simulates the LSM6DSO 6-axis IMU (accelerometer + gyroscope).
 * Generates realistic sensor data based on orientation and rotation rate.
 * Supports configurable noise, bias, and scale errors for calibration testing.
 */
class Lsm6dsoSimulator : public I2CDevice {
public:
    // Register addresses
    static constexpr uint8_t REG_WHO_AM_I   = 0x0F;
    static constexpr uint8_t REG_CTRL1_XL   = 0x10;
    static constexpr uint8_t REG_CTRL2_G    = 0x11;
    static constexpr uint8_t REG_CTRL3_C    = 0x12;
    static constexpr uint8_t REG_STATUS     = 0x1E;
    static constexpr uint8_t REG_OUT_TEMP_L = 0x20;
    static constexpr uint8_t REG_OUTX_L_G   = 0x22; // Gyro X
    static constexpr uint8_t REG_OUTY_L_G   = 0x24; // Gyro Y
    static constexpr uint8_t REG_OUTZ_L_G   = 0x26; // Gyro Z
    static constexpr uint8_t REG_OUTX_L_A   = 0x28; // Accel X
    static constexpr uint8_t REG_OUTY_L_A   = 0x2A; // Accel Y
    static constexpr uint8_t REG_OUTZ_L_A   = 0x2C; // Accel Z
    
    static constexpr uint8_t WHO_AM_I_VALUE = 0x6C;
    
    // Default sensitivities (at default ranges)
    static constexpr float DEFAULT_ACCEL_SENSITIVITY_MG = 0.061f;  // mg/LSB at 2g
    static constexpr float DEFAULT_GYRO_SENSITIVITY_MDPS = 8.75f;  // mdps/LSB at 250 dps
    
    Lsm6dsoSimulator();
    
    // I2CDevice interface
    bool readRegister(uint8_t reg, uint8_t* buf, size_t len) override;
    bool writeRegister(uint8_t reg, const uint8_t* data, size_t len) override;
    bool probe() override;
    
    // Ground truth setters
    void setOrientation(const sf::Quaternion& q);
    void setRotationRate(float wx, float wy, float wz); // rad/s
    
    // Error injection (Level B fidelity)
    void setAccelBias(const sf::Vec3& bias);      // in g
    void setGyroBias(const sf::Vec3& bias);       // in rad/s
    void setAccelNoiseStdDev(float stddev);       // in g
    void setGyroNoiseStdDev(float stddev);        // in rad/s
    void setAccelScale(const sf::Vec3& scale);    // multiplicative
    void setGyroScale(const sf::Vec3& scale);     // multiplicative
    void setSeed(uint32_t seed);

    // Temperature control
    void setTemperature(float tempC);

private:
    // Register storage
    uint8_t regCtrl1Xl_ = 0x00;
    uint8_t regCtrl2G_ = 0x00;
    uint8_t regCtrl3C_ = 0x00;
    uint8_t regStatus_ = 0x07; // XLDA=1, GDA=1, TDA=1 (data available)
    
    // Ground truth state
    sf::Quaternion orientation_ = {1.0f, 0.0f, 0.0f, 0.0f};
    sf::Vec3 rotationRate_ = {0.0f, 0.0f, 0.0f}; // rad/s
    float temperature_ = 25.0f; // Celsius
    
    // Error parameters (Level B fidelity)
    sf::Vec3 accelBias_ = {0.0f, 0.0f, 0.0f};
    sf::Vec3 gyroBias_ = {0.0f, 0.0f, 0.0f};
    sf::Vec3 accelScale_ = {1.0f, 1.0f, 1.0f};
    sf::Vec3 gyroScale_ = {1.0f, 1.0f, 1.0f};
    float accelNoiseStdDev_ = 0.0f;
    float gyroNoiseStdDev_ = 0.0f;
    
    // Random number generation for noise
    std::mt19937 rng_;
    std::normal_distribution<float> noiseDist_;
    
    // Data generation
    sf::Vec3 generateAccelData() const;
    sf::Vec3 generateGyroData() const;
    int16_t generateTempData() const;
    
    // Helper methods
    static void writeInt16(uint8_t* buf, int16_t value);
    float generateNoise(float stddev);
    
    // Get current sensitivities based on configuration
    float getAccelSensitivity() const;  // mg/LSB
    float getGyroSensitivity() const;   // mdps/LSB
};

} // namespace sim
