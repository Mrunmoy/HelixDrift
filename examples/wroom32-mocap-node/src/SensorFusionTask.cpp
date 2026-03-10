#include "SensorFusionTask.hpp"

#ifdef ESP32_STUB
#   include <cstdio>
#   define SF_LOGI(fmt, ...) printf("[I SF] " fmt "\n", ##__VA_ARGS__)
#   define SF_LOGW(fmt, ...) printf("[W SF] " fmt "\n", ##__VA_ARGS__)
#   define SF_LOGE(fmt, ...) fprintf(stderr, "[E SF] " fmt "\n", ##__VA_ARGS__)
#else
#   include "esp_log.h"
#   include "freertos/FreeRTOS.h"
#   include "freertos/task.h"
    static const char* kTag = "sf_task";
#   define SF_LOGI(fmt, ...) ESP_LOGI(kTag, fmt, ##__VA_ARGS__)
#   define SF_LOGW(fmt, ...) ESP_LOGW(kTag, fmt, ##__VA_ARGS__)
#   define SF_LOGE(fmt, ...) ESP_LOGE(kTag, fmt, ##__VA_ARGS__)
#endif

namespace helix
{

SensorFusionTask::SensorFusionTask(sf::IAccelGyroSensor&     imu,
                                   sf::IMagSensor*           mag,
                                   const SensorFusionConfig& cfg)
    : m_imu(imu)
    , m_mag(mag)
    , m_cfg(cfg)
    , m_ahrs(cfg.mahonyKp, cfg.mahonyKi)
{
}

void SensorFusionTask::start()
{
#ifndef ESP32_STUB
    xTaskCreate(taskEntry,
                "sf_task",
                /*stack=*/4096,
                /*arg=*/this,
                /*priority=*/5,
                /*handle=*/nullptr);
#endif
}

void SensorFusionTask::tick(float dt)
{
    sf::AccelData accel{};
    sf::GyroData  gyro{};

    if (!m_imu.readAccel(accel))
    {
        SF_LOGW("accel read failed on tick %d", m_tick);
    }

    if (!m_imu.readGyro(gyro))
    {
        SF_LOGW("gyro read failed on tick %d", m_tick);
    }

    if (m_mag && (m_tick % m_cfg.magDecim == 0))
    {
        sf::MagData fresh{};
        if (m_mag->readMag(fresh))
        {
            m_lastMag = fresh;
        }
        else
        {
            SF_LOGW("mag read failed on tick %d", m_tick);
        }
    }

    if (m_mag)
    {
        m_ahrs.update(accel, gyro, m_lastMag, dt);
    }
    else
    {
        m_ahrs.update6DOF(accel, gyro, dt);
    }

    if (m_tick % m_cfg.logDecim == 0)
    {
        const sf::Quaternion q = m_ahrs.getQuaternion();
        SF_LOGI("q w=%.4f x=%.4f y=%.4f z=%.4f", q.w, q.x, q.y, q.z);
    }

    ++m_tick;
}

sf::Quaternion SensorFusionTask::getQuaternion() const
{
    return m_ahrs.getQuaternion();
}

// ── FreeRTOS task body (compiled for target only) ─────────────────────────

#ifndef ESP32_STUB

void SensorFusionTask::taskEntry(void* pvArg)
{
    static_cast<SensorFusionTask*>(pvArg)->run();
}

void SensorFusionTask::run()
{
    const float      dt     = 1.0f / m_cfg.sampleRateHz;
    const TickType_t period = pdMS_TO_TICKS(1000.0f / m_cfg.sampleRateHz);
    TickType_t       last   = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&last, period);
        tick(dt);
    }
}

#endif // !ESP32_STUB

} // namespace helix
