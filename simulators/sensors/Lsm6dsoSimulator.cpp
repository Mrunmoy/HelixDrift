#include "Lsm6dsoSimulator.hpp"
#include <cmath>
#include <chrono>

namespace sim {

Lsm6dsoSimulator::Lsm6dsoSimulator()
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()),
      noiseDist_(0.0f, 1.0f)
{
}

bool Lsm6dsoSimulator::probe() {
    return true;
}

bool Lsm6dsoSimulator::readRegister(uint8_t reg, uint8_t* buf, size_t len) {
    if (!buf || len == 0) {
        return false;
    }
    
    for (size_t i = 0; i < len; i++) {
        uint8_t currentReg = reg + i;
        
        switch (currentReg) {
            case REG_WHO_AM_I:
                buf[i] = WHO_AM_I_VALUE;
                break;
                
            case REG_CTRL1_XL:
                buf[i] = regCtrl1Xl_;
                break;
                
            case REG_CTRL2_G:
                buf[i] = regCtrl2G_;
                break;
                
            case REG_CTRL3_C:
                buf[i] = regCtrl3C_;
                break;
                
            case REG_STATUS:
                buf[i] = regStatus_;
                break;
                
            // Temperature: 0x20 (L), 0x21 (H)
            case REG_OUT_TEMP_L:
                writeInt16(&buf[i], generateTempData());
                i++; // Skip next byte (already written)
                break;
            case REG_OUT_TEMP_L + 1:
                // Handled above
                break;
                
            // Gyro X: 0x22 (L), 0x23 (H)
            case REG_OUTX_L_G: {
                sf::Vec3 gyro = generateGyroData();
                writeInt16(&buf[i], static_cast<int16_t>(gyro.x));
                i++;
                break;
            }
            case REG_OUTX_L_G + 1:
                // Handled above
                break;
                
            // Gyro Y: 0x24 (L), 0x25 (H)
            case REG_OUTY_L_G: {
                sf::Vec3 gyro = generateGyroData();
                writeInt16(&buf[i], static_cast<int16_t>(gyro.y));
                i++;
                break;
            }
            case REG_OUTY_L_G + 1:
                // Handled above
                break;
                
            // Gyro Z: 0x26 (L), 0x27 (H)
            case REG_OUTZ_L_G: {
                sf::Vec3 gyro = generateGyroData();
                writeInt16(&buf[i], static_cast<int16_t>(gyro.z));
                i++;
                break;
            }
            case REG_OUTZ_L_G + 1:
                // Handled above
                break;
                
            // Accel X: 0x28 (L), 0x29 (H)
            case REG_OUTX_L_A: {
                sf::Vec3 accel = generateAccelData();
                writeInt16(&buf[i], static_cast<int16_t>(accel.x));
                i++;
                break;
            }
            case REG_OUTX_L_A + 1:
                // Handled above
                break;
                
            // Accel Y: 0x2A (L), 0x2B (H)
            case REG_OUTY_L_A: {
                sf::Vec3 accel = generateAccelData();
                writeInt16(&buf[i], static_cast<int16_t>(accel.y));
                i++;
                break;
            }
            case REG_OUTY_L_A + 1:
                // Handled above
                break;
                
            // Accel Z: 0x2C (L), 0x2D (H)
            case REG_OUTZ_L_A: {
                sf::Vec3 accel = generateAccelData();
                writeInt16(&buf[i], static_cast<int16_t>(accel.z));
                i++;
                break;
            }
            case REG_OUTZ_L_A + 1:
                // Handled above
                break;
                
            default:
                buf[i] = 0x00;
                break;
        }
    }
    
    return true;
}

bool Lsm6dsoSimulator::writeRegister(uint8_t reg, const uint8_t* data, size_t len) {
    if (!data) {
        return false;
    }
    
    for (size_t i = 0; i < len; i++) {
        uint8_t currentReg = reg + i;
        
        switch (currentReg) {
            case REG_CTRL1_XL:
                regCtrl1Xl_ = data[i];
                break;
                
            case REG_CTRL2_G:
                regCtrl2G_ = data[i];
                break;
                
            case REG_CTRL3_C:
                regCtrl3C_ = data[i];
                break;
                
            default:
                // Ignore writes to other registers
                break;
        }
    }
    
    return true;
}

void Lsm6dsoSimulator::setOrientation(const sf::Quaternion& q) {
    orientation_ = q;
}

void Lsm6dsoSimulator::setRotationRate(float wx, float wy, float wz) {
    rotationRate_ = {wx, wy, wz};
}

void Lsm6dsoSimulator::setAccelBias(const sf::Vec3& bias) {
    accelBias_ = bias;
}

void Lsm6dsoSimulator::setGyroBias(const sf::Vec3& bias) {
    gyroBias_ = bias;
}

void Lsm6dsoSimulator::setAccelNoiseStdDev(float stddev) {
    accelNoiseStdDev_ = stddev;
}

void Lsm6dsoSimulator::setGyroNoiseStdDev(float stddev) {
    gyroNoiseStdDev_ = stddev;
}

void Lsm6dsoSimulator::setAccelScale(const sf::Vec3& scale) {
    accelScale_ = scale;
}

void Lsm6dsoSimulator::setGyroScale(const sf::Vec3& scale) {
    gyroScale_ = scale;
}

void Lsm6dsoSimulator::setTemperature(float tempC) {
    temperature_ = tempC;
}

sf::Vec3 Lsm6dsoSimulator::generateAccelData() const {
    // Gravity vector in world frame: [0, 0, 1] g
    sf::Vec3 gravityWorld = {0.0f, 0.0f, 1.0f};
    
    // Rotate gravity into sensor frame based on orientation
    sf::Vec3 gravitySensor = orientation_.rotateVector(gravityWorld);
    
    // Apply scale error
    sf::Vec3 scaled = {
        gravitySensor.x * accelScale_.x,
        gravitySensor.y * accelScale_.y,
        gravitySensor.z * accelScale_.z
    };
    
    // Add bias
    sf::Vec3 withBias = {
        scaled.x + accelBias_.x,
        scaled.y + accelBias_.y,
        scaled.z + accelBias_.z
    };
    
    // Add noise
    float noiseX = const_cast<Lsm6dsoSimulator*>(this)->generateNoise(accelNoiseStdDev_);
    float noiseY = const_cast<Lsm6dsoSimulator*>(this)->generateNoise(accelNoiseStdDev_);
    float noiseZ = const_cast<Lsm6dsoSimulator*>(this)->generateNoise(accelNoiseStdDev_);
    
    sf::Vec3 finalAccel = {
        withBias.x + noiseX,
        withBias.y + noiseY,
        withBias.z + noiseZ
    };
    
    // Convert from g to raw LSB values
    // Output is in mg (milli-g) / sensitivity
    float sens = getAccelSensitivity(); // mg/LSB
    return {
        finalAccel.x * 1000.0f / sens,
        finalAccel.y * 1000.0f / sens,
        finalAccel.z * 1000.0f / sens
    };
}

sf::Vec3 Lsm6dsoSimulator::generateGyroData() const {
    // Rotation rate in rad/s
    sf::Vec3 rate = rotationRate_;
    
    // Apply scale error
    sf::Vec3 scaled = {
        rate.x * gyroScale_.x,
        rate.y * gyroScale_.y,
        rate.z * gyroScale_.z
    };
    
    // Add bias (gyro bias is in rad/s)
    sf::Vec3 withBias = {
        scaled.x + gyroBias_.x,
        scaled.y + gyroBias_.y,
        scaled.z + gyroBias_.z
    };
    
    // Add noise
    float noiseX = const_cast<Lsm6dsoSimulator*>(this)->generateNoise(gyroNoiseStdDev_);
    float noiseY = const_cast<Lsm6dsoSimulator*>(this)->generateNoise(gyroNoiseStdDev_);
    float noiseZ = const_cast<Lsm6dsoSimulator*>(this)->generateNoise(gyroNoiseStdDev_);
    
    sf::Vec3 finalRate = {
        withBias.x + noiseX,
        withBias.y + noiseY,
        withBias.z + noiseZ
    };
    
    // Convert from rad/s to raw LSB values
    // First convert to dps (degrees per second): 1 rad/s = 180/pi dps
    constexpr float RAD_TO_DPS = 57.29577951308232f; // 180/pi
    float dpsX = finalRate.x * RAD_TO_DPS;
    float dpsY = finalRate.y * RAD_TO_DPS;
    float dpsZ = finalRate.z * RAD_TO_DPS;
    
    // Then convert to LSB: dps / (mdps/LSB * 0.001)
    float sens = getGyroSensitivity(); // mdps/LSB
    return {
        dpsX * 1000.0f / sens,
        dpsY * 1000.0f / sens,
        dpsZ * 1000.0f / sens
    };
}

int16_t Lsm6dsoSimulator::generateTempData() const {
    // Temperature conversion: raw = (tempC - 25) * 256
    // From datasheet: Temp = (raw / 256) + 25
    float delta = temperature_ - 25.0f;
    return static_cast<int16_t>(delta * 256.0f);
}

void Lsm6dsoSimulator::writeInt16(uint8_t* buf, int16_t value) {
    // Little-endian: LSB first, then MSB
    buf[0] = static_cast<uint8_t>(value & 0xFF);
    buf[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

float Lsm6dsoSimulator::generateNoise(float stddev) {
    if (stddev <= 0.0f) {
        return 0.0f;
    }
    return noiseDist_(rng_) * stddev;
}

float Lsm6dsoSimulator::getAccelSensitivity() const {
    // Get FS_XL from CTRL1_XL bits [3:2]
    // 0=2g, 1=16g, 2=4g, 3=8g
    uint8_t fs = (regCtrl1Xl_ >> 2) & 0x03;
    
    switch (fs) {
        case 0: return 0.061f;   // 2g
        case 1: return 0.488f;   // 16g
        case 2: return 0.122f;   // 4g
        case 3: return 0.244f;   // 8g
        default: return 0.061f;
    }
}

float Lsm6dsoSimulator::getGyroSensitivity() const {
    // Get FS_G from CTRL2_G bits [3:1]
    // 0=250dps, 1=125dps, 2=500dps, 4=1000dps, 6=2000dps
    uint8_t fs = (regCtrl2G_ >> 1) & 0x07;
    
    switch (fs) {
        case 0: return 8.75f;    // 250 dps
        case 1: return 4.375f;   // 125 dps
        case 2: return 17.5f;    // 500 dps
        case 4: return 35.0f;    // 1000 dps
        case 6: return 70.0f;    // 2000 dps
        default: return 8.75f;
    }
}

} // namespace sim
