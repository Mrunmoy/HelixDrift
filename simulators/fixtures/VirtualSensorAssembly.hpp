#pragma once

#include "VirtualI2CBus.hpp"
#include "Lsm6dsoSimulator.hpp"
#include "Bmm350Simulator.hpp"
#include "Lps22dfSimulator.hpp"
#include "VirtualGimbal.hpp"

#include "LSM6DSO.hpp"
#include "BMM350.hpp"
#include "LPS22DF.hpp"

#include <memory>

namespace sim {

class VirtualSensorAssembly {
public:
    struct DelayProvider : public sf::IDelayProvider {
        void delayMs(uint32_t) override {}
        void delayUs(uint32_t) override {}
        uint64_t getTimestampUs() override { return timestampUs; }

        uint64_t timestampUs = 0;
    };

    VirtualSensorAssembly()
        : imuBus_(std::make_unique<VirtualI2CBus>())
        , auxBus_(std::make_unique<VirtualI2CBus>())
        , imuSim_(std::make_unique<Lsm6dsoSimulator>())
        , magSim_(std::make_unique<Bmm350Simulator>())
        , baroSim_(std::make_unique<Lps22dfSimulator>())
        , gimbal_(std::make_unique<VirtualGimbal>())
        , delay_(std::make_unique<DelayProvider>())
        , imu_(*imuBus_, *delay_, sf::LSM6DSOConfig{})
        , mag_(*auxBus_, *delay_, sf::BMM350Config{})
        , baro_(*auxBus_, *delay_, sf::LPS22DFConfig{})
    {
        imuBus_->registerDevice(sf::LSM6DSOConfig{}.address, *imuSim_);
        auxBus_->registerDevice(sf::BMM350Config{}.address, *magSim_);
        auxBus_->registerDevice(sf::LPS22DFConfig{}.address, *baroSim_);

        gimbal_->attachAccelGyroSensor(imuSim_.get());
        gimbal_->attachMagSensor(magSim_.get());
        gimbal_->attachBaroSensor(baroSim_.get());

        resetAndSync();
    }

    void resetAndSync() {
        gimbal_->reset();
        gimbal_->syncToSensors();
    }

    bool initAll() {
        return imu_.init() && mag_.init() && baro_.init();
    }

    void advanceTimeUs(uint64_t deltaUs) {
        delay_->timestampUs += deltaUs;
    }

    VirtualI2CBus& imuBus() { return *imuBus_; }
    VirtualI2CBus& auxBus() { return *auxBus_; }

    Lsm6dsoSimulator& imuSim() { return *imuSim_; }
    Bmm350Simulator& magSim() { return *magSim_; }
    Lps22dfSimulator& baroSim() { return *baroSim_; }
    VirtualGimbal& gimbal() { return *gimbal_; }
    DelayProvider& delay() { return *delay_; }

    sf::LSM6DSO& imuDriver() { return imu_; }
    sf::BMM350& magDriver() { return mag_; }
    sf::LPS22DF& baroDriver() { return baro_; }

private:
    std::unique_ptr<VirtualI2CBus> imuBus_;
    std::unique_ptr<VirtualI2CBus> auxBus_;
    std::unique_ptr<Lsm6dsoSimulator> imuSim_;
    std::unique_ptr<Bmm350Simulator> magSim_;
    std::unique_ptr<Lps22dfSimulator> baroSim_;
    std::unique_ptr<VirtualGimbal> gimbal_;
    std::unique_ptr<DelayProvider> delay_;
    sf::LSM6DSO imu_;
    sf::BMM350 mag_;
    sf::LPS22DF baro_;
};

} // namespace sim
