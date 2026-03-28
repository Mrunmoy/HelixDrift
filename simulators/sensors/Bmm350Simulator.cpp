#include "Bmm350Simulator.hpp"
#include <cstring>
#include <cmath>

namespace sim {

Bmm350Simulator::Bmm350Simulator() : rng_(42) {
    // Initialize registers with defaults
    std::memset(registers_, 0, sizeof(registers_));
    std::memset(otpData_, 0, sizeof(otpData_));
    
    // CHIP_ID is read-only
    registers_[REG_CHIP_ID] = CHIP_ID_VALUE;
    
    // Default PMU status is suspended
    registers_[REG_PMU_CMD_STATUS] = 0x00;
    
    // Default OTP status
    registers_[REG_OTP_STATUS] = 0x01;  // Data valid by default
}

bool Bmm350Simulator::probe() {
    return true;
}

bool Bmm350Simulator::readRegister(uint8_t reg, uint8_t* buf, size_t len) {
    if (!buf || len == 0) {
        return false;
    }
    
    // Handle special registers that need dynamic updates
    if (reg == REG_MAG_X_XLSB && len >= 9) {
        // Update mag data before reading
        updateMagData();
    } else if (reg == REG_TEMP_XLSB && len >= 3) {
        // Update temperature before reading
        updateTemperatureData();
    }
    
    // Read from registers
    for (size_t i = 0; i < len; i++) {
        uint8_t addr = reg + static_cast<uint8_t>(i);
        if (addr < 256) {
            buf[i] = registers_[addr];
        } else {
            buf[i] = 0;
        }
    }
    
    return true;
}

bool Bmm350Simulator::writeRegister(uint8_t reg, const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return false;
    }
    
    for (size_t i = 0; i < len; i++) {
        uint8_t addr = reg + static_cast<uint8_t>(i);
        if (addr >= 256) {
            continue;
        }
        
        // CHIP_ID is read-only
        if (addr == REG_CHIP_ID) {
            continue;
        }
        
        // Handle special registers
        if (addr == REG_OTP_CMD) {
            handleOtpCommand(data[i]);
        }
        
        registers_[addr] = data[i];
    }
    
    return true;
}

void Bmm350Simulator::setOrientation(const sf::Quaternion& q) {
    orientation_ = q;
    orientation_.normalize();
}

void Bmm350Simulator::setEarthField(const sf::Vec3& field) {
    earthField_ = field;
}

void Bmm350Simulator::setTemperature(float tempC) {
    temperature_ = tempC;
}

void Bmm350Simulator::setErrors(const ErrorConfig& errors) {
    errors_ = errors;
    if (errors.noiseStdDev > 0) {
        noiseDist_ = std::normal_distribution<float>(0.0f, errors.noiseStdDev);
    }
}

void Bmm350Simulator::setOtpData(uint8_t wordAddr, uint16_t data) {
    if (wordAddr < 32) {
        otpData_[wordAddr] = data;
    }
}

uint16_t Bmm350Simulator::getOtpData(uint8_t wordAddr) const {
    if (wordAddr < 32) {
        return otpData_[wordAddr];
    }
    return 0;
}

sf::Vec3 Bmm350Simulator::getRawMagData() const {
    // Rotate earth field into sensor frame
    return orientation_.rotateVector(earthField_);
}

float Bmm350Simulator::getRawTemperature() const {
    return temperature_;
}

sf::Vec3 Bmm350Simulator::getMagData() const {
    // Get the raw (rotated) earth field
    sf::Vec3 raw = getRawMagData();
    
    // Apply soft iron scaling
    sf::Vec3 scaled;
    scaled.x = raw.x * errors_.softIronScale.x;
    scaled.y = raw.y * errors_.softIronScale.y;
    scaled.z = raw.z * errors_.softIronScale.z;
    
    // Add hard iron and bias
    sf::Vec3 result;
    result.x = scaled.x + errors_.hardIron.x + errors_.bias.x;
    result.y = scaled.y + errors_.hardIron.y + errors_.bias.y;
    result.z = scaled.z + errors_.hardIron.z + errors_.bias.z;
    
    // Add noise if configured
    if (errors_.noiseStdDev > 0.0f) {
        result.x += generateNoise();
        result.y += generateNoise();
        result.z += generateNoise();
    }
    
    return result;
}

void Bmm350Simulator::updateMagData() {
    sf::Vec3 mag = getMagData();
    
    // Encode mag values to raw sensor format
    uint32_t rawX = encodeMagToRaw(mag.x, UT_SCALE_XY);
    uint32_t rawY = encodeMagToRaw(mag.y, UT_SCALE_XY);
    uint32_t rawZ = encodeMagToRaw(mag.z, UT_SCALE_Z);
    
    // Store in registers (24-bit little-endian, starting at MAG_X_XLSB)
    // X axis
    registers_[REG_MAG_X_XLSB + 0] = static_cast<uint8_t>(rawX & 0xFF);
    registers_[REG_MAG_X_XLSB + 1] = static_cast<uint8_t>((rawX >> 8) & 0xFF);
    registers_[REG_MAG_X_XLSB + 2] = static_cast<uint8_t>((rawX >> 16) & 0xFF);
    
    // Y axis
    registers_[REG_MAG_X_XLSB + 3] = static_cast<uint8_t>(rawY & 0xFF);
    registers_[REG_MAG_X_XLSB + 4] = static_cast<uint8_t>((rawY >> 8) & 0xFF);
    registers_[REG_MAG_X_XLSB + 5] = static_cast<uint8_t>((rawY >> 16) & 0xFF);
    
    // Z axis
    registers_[REG_MAG_X_XLSB + 6] = static_cast<uint8_t>(rawZ & 0xFF);
    registers_[REG_MAG_X_XLSB + 7] = static_cast<uint8_t>((rawZ >> 8) & 0xFF);
    registers_[REG_MAG_X_XLSB + 8] = static_cast<uint8_t>((rawZ >> 16) & 0xFF);
}

void Bmm350Simulator::updateTemperatureData() {
    uint32_t rawTemp = encodeTempToRaw(temperature_);
    
    // Store in registers (24-bit little-endian, starting at TEMP_XLSB)
    registers_[REG_TEMP_XLSB + 0] = static_cast<uint8_t>(rawTemp & 0xFF);
    registers_[REG_TEMP_XLSB + 1] = static_cast<uint8_t>((rawTemp >> 8) & 0xFF);
    registers_[REG_TEMP_XLSB + 2] = static_cast<uint8_t>((rawTemp >> 16) & 0xFF);
}

uint32_t Bmm350Simulator::encodeMagToRaw(float valueUT, float scale) {
    // Convert µT to raw sensor value
    // raw = value * scale
    float raw = valueUT * scale;
    
    // Clamp to 21-bit signed range
    if (raw > 1048575.0f) raw = 1048575.0f;
    if (raw < -1048576.0f) raw = -1048576.0f;
    
    int32_t rawInt = static_cast<int32_t>(raw);
    
    // Mask to 21 bits
    return static_cast<uint32_t>(rawInt & 0x1FFFFF);
}

uint32_t Bmm350Simulator::encodeTempToRaw(float tempC) {
    // Inverse of: tempC = raw * TEMP_SCALE + TEMP_OFFSET
    // raw = (tempC - TEMP_OFFSET) / TEMP_SCALE
    float raw = (tempC - TEMP_OFFSET) / TEMP_SCALE;
    
    // Clamp to 21-bit signed range
    if (raw > 1048575.0f) raw = 1048575.0f;
    if (raw < -1048576.0f) raw = -1048576.0f;
    
    int32_t rawInt = static_cast<int32_t>(raw);
    
    // Mask to 21 bits
    return static_cast<uint32_t>(rawInt & 0x1FFFFF);
}

float Bmm350Simulator::generateNoise() const {
    if (errors_.noiseStdDev <= 0.0f) {
        return 0.0f;
    }
    return noiseDist_(rng_);
}

void Bmm350Simulator::handleOtpCommand(uint8_t cmd) {
    lastOtpCmd_ = cmd;
    
    // Check if this is a read command (bit 5 = DIR_READ)
    if (cmd & 0x20) {
        uint8_t wordAddr = cmd & 0x1F;
        if (wordAddr < 32) {
            // Update OTP data registers
            uint16_t data = otpData_[wordAddr];
            registers_[REG_OTP_DATA_MSB] = static_cast<uint8_t>(data >> 8);
            registers_[REG_OTP_DATA_LSB] = static_cast<uint8_t>(data & 0xFF);
            registers_[REG_OTP_STATUS] = 0x01;  // Data valid
        } else {
            registers_[REG_OTP_STATUS] = 0x00;  // Error
        }
    }
}

void Bmm350Simulator::updateOtpDataRegisters() {
    // Populate registers 0x04-0x0A with OTP calibration data
    // This is a simplified mapping for simulation purposes
    
    // 0x04-0x0A: OTP data area (7 bytes)
    // Can store various calibration parameters
    
    // For now, populate with some default calibration values
    // These would normally come from actual OTP programming
    
    // Example: Store offset and sensitivity data
    int16_t offsetX = static_cast<int16_t>(errors_.hardIron.x * 10.0f);
    int16_t offsetY = static_cast<int16_t>(errors_.hardIron.y * 10.0f);
    
    registers_[0x04] = static_cast<uint8_t>(offsetX & 0xFF);
    registers_[0x05] = static_cast<uint8_t>((offsetX >> 8) & 0xFF);
    registers_[0x06] = static_cast<uint8_t>(offsetY & 0xFF);
    registers_[0x07] = static_cast<uint8_t>((offsetY >> 8) & 0xFF);
    registers_[0x08] = 0x00;
    registers_[0x09] = 0x00;
    registers_[0x0A] = 0x00;
}

} // namespace sim
