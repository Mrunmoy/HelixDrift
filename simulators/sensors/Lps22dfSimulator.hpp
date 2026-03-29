#pragma once

#include "VirtualI2CBus.hpp"
#include <cstdint>
#include <random>

namespace sim {

/**
 * @brief LPS22DF Barometric Pressure Sensor Simulator
 * 
 * Simulates the STMicroelectronics LPS22DF MEMS pressure sensor.
 * Provides pressure readings based on altitude using barometric formula,
 * with configurable noise and bias for testing.
 * 
 * Register Map:
 * - 0x0F WHO_AM_I      - Device ID (0xB4)
 * - 0x10 CTRL_REG1     - Control register 1 (ODR, AVG)
 * - 0x12 CTRL_REG3     - Control register 3 (IF_ADD_INC)
 * - 0x0B STATUS        - Status register
 * - 0x28-0x2A PRESS_OUT_XL/L/H - Pressure data (24-bit little-endian)
 * - 0x2B-0x2C TEMP_OUT_L/H     - Temperature data (16-bit little-endian)
 */
class Lps22dfSimulator : public I2CDevice {
public:
    /**
     * @brief Default constructor
     */
    Lps22dfSimulator();
    
    /**
     * @brief Destructor
     */
    ~Lps22dfSimulator() override = default;
    
    // I2CDevice interface
    bool readRegister(uint8_t reg, uint8_t* buf, size_t len) override;
    bool writeRegister(uint8_t reg, const uint8_t* data, size_t len) override;
    bool probe() override;
    
    /**
     * @brief Set the current altitude
     * @param meters Altitude in meters above sea level
     * 
     * Pressure is calculated using the barometric formula:
     * P = P0 * (1 - H/44330)^5.255
     * where P0 is the base pressure at sea level.
     */
    void setAltitude(float meters);
    
    /**
     * @brief Override the calculated pressure
     * @param hPa Pressure in hectopascals (hPa)
     * 
     * This overrides the altitude-based calculation.
     * Use setAltitude() to return to altitude-based mode.
     */
    void setPressure(float hPa);
    
    /**
     * @brief Set the base pressure at sea level
     * @param hPa Base pressure in hectopascals (default: 1013.25)
     */
    void setBasePressure(float hPa);
    
    /**
     * @brief Set the temperature
     * @param tempC Temperature in degrees Celsius
     * 
     * Overrides the default temperature.
     */
    void setTemperature(float tempC);
    
    /**
     * @brief Set pressure bias error
     * @param bias Bias in hPa to add to all pressure readings
     */
    void setPressureBias(float bias);
    
    /**
     * @brief Set pressure noise standard deviation
     * @param stdDev Standard deviation in hPa
     */
    void setPressureNoiseStdDev(float stdDev);
    
    /**
     * @brief Set temperature bias error
     * @param bias Bias in °C to add to all temperature readings
     */
    void setTemperatureBias(float bias);
    void setSeed(uint32_t seed);
    
    /**
     * @brief Get current altitude
     * @return Current altitude in meters
     */
    float getAltitude() const { return altitude_; }
    
    /**
     * @brief Get current pressure
     * @return Current pressure in hPa
     */
    float getPressure() const;
    
    /**
     * @brief Get current temperature
     * @return Current temperature in °C
     */
    float getTemperature() const;

private:
    // Register addresses
    static constexpr uint8_t WHO_AM_I = 0x0F;
    static constexpr uint8_t CTRL_REG1 = 0x10;
    static constexpr uint8_t CTRL_REG2 = 0x11;
    static constexpr uint8_t CTRL_REG3 = 0x12;
    static constexpr uint8_t CTRL_REG4 = 0x13;
    static constexpr uint8_t STATUS = 0x0B;
    static constexpr uint8_t PRESS_OUT_XL = 0x28;
    static constexpr uint8_t PRESS_OUT_L = 0x29;
    static constexpr uint8_t PRESS_OUT_H = 0x2A;
    static constexpr uint8_t TEMP_OUT_L = 0x2B;
    static constexpr uint8_t TEMP_OUT_H = 0x2C;
    
    // Device ID
    static constexpr uint8_t DEVICE_ID = 0xB4;
    
    // Sensor sensitivities
    static constexpr float PRESS_SENSITIVITY = 4096.0f;  // LSB/hPa
    static constexpr float TEMP_SENSITIVITY = 100.0f;    // LSB/°C
    
    // Register storage
    uint8_t ctrlReg1_ = 0x00;
    uint8_t ctrlReg2_ = 0x00;
    uint8_t ctrlReg3_ = 0x00;
    uint8_t ctrlReg4_ = 0x00;
    uint8_t status_ = 0x03;  // P_DA and T_DA initially set
    
    // Ground truth state
    float altitude_ = 0.0f;
    float basePressure_ = 1013.25f;  // Sea level pressure
    float temperature_ = 22.0f;      // Default room temperature
    
    // Error injection
    float pressureBias_ = 0.0f;
    float pressureNoiseStdDev_ = 0.0f;
    float temperatureBias_ = 0.0f;
    
    // Pressure override (when setPressure is called)
    bool pressureOverride_ = false;
    float pressureOverrideValue_ = 1013.25f;
    
    // Temperature override
    bool temperatureOverride_ = false;
    float temperatureOverrideValue_ = 22.0f;
    
    // Random number generation for noise
    std::mt19937 rng_;
    std::normal_distribution<float> noiseDist_;
    
    // Helper methods
    float calculatePressureFromAltitude(float altitude) const;
    int32_t getRawPressure() const;
    int16_t getRawTemperature() const;
    float generatePressureNoise() const;
};

} // namespace sim
