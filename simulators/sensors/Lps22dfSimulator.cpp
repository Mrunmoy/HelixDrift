#include "Lps22dfSimulator.hpp"
#include <cmath>
#include <chrono>

namespace sim {

Lps22dfSimulator::Lps22dfSimulator()
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count())
    , noiseDist_(0.0f, 1.0f)
{
}

bool Lps22dfSimulator::readRegister(uint8_t reg, uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t currentReg = reg + static_cast<uint8_t>(i);
        
        switch (currentReg) {
            case WHO_AM_I:
                buf[i] = DEVICE_ID;
                break;
                
            case CTRL_REG1:
                buf[i] = ctrlReg1_;
                break;
                
            case CTRL_REG2:
                buf[i] = ctrlReg2_;
                break;
                
            case CTRL_REG3:
                buf[i] = ctrlReg3_;
                break;
                
            case CTRL_REG4:
                buf[i] = ctrlReg4_;
                break;
                
            case STATUS:
                // Bit 0: T_DA (temperature data available)
                // Bit 1: P_DA (pressure data available)
                buf[i] = status_;
                break;
                
            case PRESS_OUT_XL:
                buf[i] = static_cast<uint8_t>(getRawPressure() & 0xFF);
                break;
                
            case PRESS_OUT_L:
                buf[i] = static_cast<uint8_t>((getRawPressure() >> 8) & 0xFF);
                break;
                
            case PRESS_OUT_H:
                buf[i] = static_cast<uint8_t>((getRawPressure() >> 16) & 0xFF);
                break;
                
            case TEMP_OUT_L:
                buf[i] = static_cast<uint8_t>(getRawTemperature() & 0xFF);
                break;
                
            case TEMP_OUT_H:
                buf[i] = static_cast<uint8_t>((getRawTemperature() >> 8) & 0xFF);
                break;
                
            default:
                // Unknown register returns 0
                buf[i] = 0x00;
                break;
        }
    }
    return true;
}

bool Lps22dfSimulator::writeRegister(uint8_t reg, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t currentReg = reg + static_cast<uint8_t>(i);
        
        switch (currentReg) {
            case CTRL_REG1:
                ctrlReg1_ = data[i];
                break;
                
            case CTRL_REG2:
                ctrlReg2_ = data[i];
                // Handle software reset (bit 2)
                if (ctrlReg2_ & 0x04) {
                    // Software reset - clear registers
                    ctrlReg1_ = 0x00;
                    ctrlReg3_ = 0x00;
                    ctrlReg4_ = 0x00;
                    // Clear reset bit after processing
                    ctrlReg2_ &= ~0x04;
                }
                break;
                
            case CTRL_REG3:
                ctrlReg3_ = data[i];
                break;
                
            case CTRL_REG4:
                ctrlReg4_ = data[i];
                break;
                
            default:
                // Ignore writes to read-only registers
                break;
        }
    }
    return true;
}

bool Lps22dfSimulator::probe() {
    // Device is present
    return true;
}

void Lps22dfSimulator::setAltitude(float meters) {
    altitude_ = meters;
    pressureOverride_ = false;  // Clear any pressure override
}

void Lps22dfSimulator::setPressure(float hPa) {
    pressureOverrideValue_ = hPa;
    pressureOverride_ = true;
}

void Lps22dfSimulator::setBasePressure(float hPa) {
    basePressure_ = hPa;
}

void Lps22dfSimulator::setTemperature(float tempC) {
    temperatureOverrideValue_ = tempC;
    temperatureOverride_ = true;
}

void Lps22dfSimulator::setPressureBias(float bias) {
    pressureBias_ = bias;
}

void Lps22dfSimulator::setPressureNoiseStdDev(float stdDev) {
    pressureNoiseStdDev_ = stdDev;
}

void Lps22dfSimulator::setTemperatureBias(float bias) {
    temperatureBias_ = bias;
}

float Lps22dfSimulator::getPressure() const {
    float pressure;
    if (pressureOverride_) {
        pressure = pressureOverrideValue_;
    } else {
        pressure = calculatePressureFromAltitude(altitude_);
    }
    
    // Add bias
    pressure += pressureBias_;
    
    // Add noise
    pressure += generatePressureNoise();
    
    return pressure;
}

float Lps22dfSimulator::getTemperature() const {
    float temp = temperatureOverride_ ? temperatureOverrideValue_ : temperature_;
    temp += temperatureBias_;
    return temp;
}

float Lps22dfSimulator::calculatePressureFromAltitude(float altitude) const {
    // Barometric formula: P = P0 * (1 - H/44330)^5.255
    // where P0 is sea level pressure, H is altitude in meters
    if (altitude >= 44330.0f) {
        // At very high altitudes, pressure approaches zero
        return 0.0f;
    }
    
    float ratio = 1.0f - (altitude / 44330.0f);
    float pressure = basePressure_ * std::pow(ratio, 5.255f);
    
    return pressure;
}

int32_t Lps22dfSimulator::getRawPressure() const {
    float pressure = getPressure();
    
    // Convert to raw 24-bit value
    // Using 4096 LSB/hPa sensitivity
    float rawFloat = pressure * PRESS_SENSITIVITY;
    
    // Clamp to 24-bit signed range
    if (rawFloat > 8388607.0f) rawFloat = 8388607.0f;   // Max positive 24-bit
    if (rawFloat < -8388608.0f) rawFloat = -8388608.0f; // Min negative 24-bit
    
    return static_cast<int32_t>(rawFloat);
}

int16_t Lps22dfSimulator::getRawTemperature() const {
    float temp = getTemperature();
    
    // Convert to raw 16-bit value
    // Using 100 LSB/°C sensitivity
    float rawFloat = temp * TEMP_SENSITIVITY;
    
    // Clamp to 16-bit signed range
    if (rawFloat > 32767.0f) rawFloat = 32767.0f;
    if (rawFloat < -32768.0f) rawFloat = -32768.0f;
    
    return static_cast<int16_t>(rawFloat);
}

float Lps22dfSimulator::generatePressureNoise() const {
    if (pressureNoiseStdDev_ <= 0.0f) {
        return 0.0f;
    }
    
    // Box-Muller transform for normal distribution
    // Using mutable RNG through const_cast since this is logically const
    auto& mutableRng = const_cast<std::mt19937&>(rng_);
    auto& mutableDist = const_cast<std::normal_distribution<float>&>(noiseDist_);
    
    return mutableDist(mutableRng) * pressureNoiseStdDev_;
}

} // namespace sim
