#pragma once

#include "CalibrationStore.hpp"
#include "HardIronCalibrator.hpp"
#include "SensorInterface.hpp"

namespace sim {

class CalibratedMagSensor : public sf::IMagSensor {
public:
    CalibratedMagSensor(sf::IMagSensor& inner,
                        sf::CalibrationStore& store,
                        const sf::CalibrationData& calibration = {});

    bool readMag(sf::MagData& out) override;

    void setCalibration(const sf::CalibrationData& calibration);
    const sf::CalibrationData& getCalibration() const { return calibration_; }
    void clearCalibration();
    void applyHardIronEstimate(const HardIronCalibrator& calibrator);

private:
    sf::IMagSensor& inner_;
    sf::CalibrationStore& store_;
    sf::CalibrationData calibration_{};
};

} // namespace sim
