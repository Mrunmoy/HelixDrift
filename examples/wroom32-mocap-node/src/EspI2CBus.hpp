#pragma once

#include "II2CBus.hpp"

#include "driver/i2c_master.h"
#include "hal/gpio_types.h"

#include <cstdint>
#include <cstddef>

namespace helix
{

/**
 * II2CBus implementation backed by the ESP-IDF i2c_master driver (IDF ≥ 5.x).
 *
 * A single bus handle is created at construction time. Device handles are
 * registered lazily on first access, keyed by 7-bit I2C address. Up to
 * kMaxDevices distinct addresses may be used over the lifetime of the bus.
 *
 * All transactions use a blocking timeout of kTimeoutMs milliseconds.
 */
class EspI2CBus final : public sf::II2CBus
{
public:
    static constexpr size_t kMaxDevices = 8;
    static constexpr int    kTimeoutMs  = 50;

    struct Config
    {
        i2c_port_num_t port    = I2C_NUM_0;
        gpio_num_t     sdaPin  = GPIO_NUM_32;
        gpio_num_t     sclPin  = GPIO_NUM_33;
        uint32_t       clkHz   = 400'000;
    };

    explicit EspI2CBus(const Config& cfg);
    ~EspI2CBus();

    // Non-copyable, non-movable — owns hardware resources.
    EspI2CBus(const EspI2CBus&)            = delete;
    EspI2CBus& operator=(const EspI2CBus&) = delete;

    bool readRegister(uint8_t devAddr, uint8_t reg,
                      uint8_t* buf, size_t len) override;

    bool writeRegister(uint8_t devAddr, uint8_t reg,
                       const uint8_t* data, size_t len) override;

    bool probe(uint8_t devAddr) override;

private:
    struct DevEntry
    {
        uint8_t                 addr;
        i2c_master_dev_handle_t handle;
    };

    Config                  m_cfg;
    i2c_master_bus_handle_t m_bus = nullptr;
    DevEntry                m_devices[kMaxDevices]{};
    size_t                  m_devCount = 0;
    uint32_t                m_clkHz;

    i2c_master_dev_handle_t getOrAddDevice(uint8_t devAddr);
};

} // namespace helix
