#include "CalibratedMagSensor.hpp"

namespace sim {

CalibratedMagSensor::CalibratedMagSensor(sf::IMagSensor& inner,
                                         sf::CalibrationStore& store,
                                         const sf::CalibrationData& calibration)
    : inner_(inner)
    , store_(store)
    , calibration_(calibration) {}

bool CalibratedMagSensor::readMag(sf::MagData& out) {
    if (!inner_.readMag(out)) {
        return false;
    }
    store_.apply(calibration_, out);
    return true;
}

void CalibratedMagSensor::setCalibration(const sf::CalibrationData& calibration) {
    calibration_ = calibration;
}

void CalibratedMagSensor::clearCalibration() {
    calibration_ = {};
}

void CalibratedMagSensor::applyHardIronEstimate(const HardIronCalibrator& calibrator) {
    const sf::Vec3 offset = calibrator.getOffset();
    calibration_.offsetX = offset.x;
    calibration_.offsetY = offset.y;
    calibration_.offsetZ = offset.z;
    calibration_.scaleX = 1.0f;
    calibration_.scaleY = 1.0f;
    calibration_.scaleZ = 1.0f;
}

} // namespace sim
