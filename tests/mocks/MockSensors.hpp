#pragma once

#include "SensorInterface.hpp"
#include "IDelayProvider.hpp"
#include <gmock/gmock.h>

namespace helix::mocks
{

class MockAccelGyro : public sf::IAccelGyroSensor
{
public:
    MOCK_METHOD(bool, readAccel,       (sf::AccelData&), (override));
    MOCK_METHOD(bool, readGyro,        (sf::GyroData&),  (override));
    MOCK_METHOD(bool, readTemperature, (float&),         (override));
};

class MockMag : public sf::IMagSensor
{
public:
    MOCK_METHOD(bool, readMag, (sf::MagData&), (override));
};

} // namespace helix::mocks
