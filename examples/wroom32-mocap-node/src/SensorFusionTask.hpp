#pragma once

#include "SensorInterface.hpp"
#include "MahonyAHRS.hpp"
#include "SensorTypes.hpp"

namespace helix
{

/** Tuning parameters for SensorFusionTask. */
struct SensorFusionConfig
{
    float sampleRateHz = 200.0f;
    float mahonyKp     = 1.0f;
    float mahonyKi     = 0.01f;
    int   magDecim     = 4;   ///< Read mag every N ticks (≈50 Hz at 200 Hz base)
    int   logDecim     = 20;  ///< Log quaternion every N ticks (≈10 Hz)
};

/**
 * Sensor fusion task — runs Mahony AHRS at a fixed sample rate.
 *
 * Dependencies are injected as abstract interfaces, enabling full host-side
 * unit testing without any ESP-IDF or FreeRTOS linkage.
 *
 * 9-DOF mode: mag != nullptr and mag read succeeds on at least one tick.
 * 6-DOF mode: mag == nullptr (caller's choice).
 *
 * Usage (ESP32 target):
 *   SensorFusionTask task{imu, &mag};
 *   task.start();   // spawns FreeRTOS task
 *
 * Usage (host unit tests):
 *   SensorFusionTask task{mockImu, &mockMag};
 *   task.tick(dt);  // drive directly — no FreeRTOS required
 */
class SensorFusionTask
{
public:
    /**
     * @param imu   Accel + gyro sensor (must outlive this object).
     * @param mag   Magnetometer, or nullptr for 6-DOF-only operation.
     * @param cfg   Tuning parameters.
     */
    SensorFusionTask(sf::IAccelGyroSensor&      imu,
                     sf::IMagSensor*            mag,
                     const SensorFusionConfig&  cfg = SensorFusionConfig{});

    /** Spawn the FreeRTOS task. Call once after board init. */
    void start();

    /**
     * Execute one sensor fusion iteration.
     *
     * Reads accel + gyro every call; reads mag every cfg.magDecim calls.
     * Exposed for unit testing — do not call from application code.
     *
     * @param dt  Time step in seconds (1 / sampleRateHz).
     */
    void tick(float dt);

    sf::Quaternion getQuaternion() const;
    bool           isMagEnabled()  const { return m_mag != nullptr; }

private:
    sf::IAccelGyroSensor& m_imu;
    sf::IMagSensor*       m_mag;
    SensorFusionConfig    m_cfg;
    sf::MahonyAHRS        m_ahrs;
    sf::MagData           m_lastMag{};
    int                   m_tick = 0;

    static void taskEntry(void* pvArg);
    void        run();
};

} // namespace helix
