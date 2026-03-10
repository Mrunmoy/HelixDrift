#include "EspI2CBus.hpp"

#include "esp_log.h"

#include <cstring>

static const char* TAG = "EspI2CBus";

namespace helix
{

EspI2CBus::EspI2CBus(const Config& cfg)
    : m_cfg(cfg)
    , m_clkHz(cfg.clkHz)
{
    i2c_master_bus_config_t busCfg = {};
    busCfg.i2c_port              = m_cfg.port;
    busCfg.sda_io_num            = m_cfg.sdaPin;
    busCfg.scl_io_num            = m_cfg.sclPin;
    busCfg.clk_source            = I2C_CLK_SRC_DEFAULT;
    busCfg.glitch_ignore_cnt     = 7;
    busCfg.flags.enable_internal_pullup = false; // rely on external pull-ups

    const esp_err_t err = i2c_new_master_bus(&busCfg, &m_bus);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %d", err);
        m_bus = nullptr;
    }
}

EspI2CBus::~EspI2CBus()
{
    if (m_bus)
    {
        // Device handles are automatically freed when the bus is deleted.
        i2c_del_master_bus(m_bus);
        m_bus = nullptr;
    }
}

bool EspI2CBus::readRegister(uint8_t devAddr, uint8_t reg,
                              uint8_t* buf, size_t len)
{
    i2c_master_dev_handle_t dev = getOrAddDevice(devAddr);
    if (!dev) return false;

    const esp_err_t err =
        i2c_master_transmit_receive(dev, &reg, 1, buf, len, kTimeoutMs);
    return err == ESP_OK;
}

bool EspI2CBus::writeRegister(uint8_t devAddr, uint8_t reg,
                               const uint8_t* data, size_t len)
{
    i2c_master_dev_handle_t dev = getOrAddDevice(devAddr);
    if (!dev) return false;

    // Build a single buffer: [reg, data[0], data[1], ...]
    constexpr size_t kMaxWrite = 32;
    if (len + 1 > kMaxWrite) return false;

    uint8_t buf[kMaxWrite];
    buf[0] = reg;
    memcpy(buf + 1, data, len);

    const esp_err_t err =
        i2c_master_transmit(dev, buf, len + 1, kTimeoutMs);
    return err == ESP_OK;
}

bool EspI2CBus::probe(uint8_t devAddr)
{
    if (!m_bus) return false;
    return i2c_master_probe(m_bus, devAddr, kTimeoutMs) == ESP_OK;
}

i2c_master_dev_handle_t EspI2CBus::getOrAddDevice(uint8_t devAddr)
{
    if (!m_bus) return nullptr;

    for (size_t i = 0; i < m_devCount; ++i)
    {
        if (m_devices[i].addr == devAddr) return m_devices[i].handle;
    }

    if (m_devCount >= kMaxDevices)
    {
        ESP_LOGE(TAG, "device table full (max %zu)", kMaxDevices);
        return nullptr;
    }

    i2c_device_config_t devCfg = {};
    devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devCfg.device_address  = devAddr;
    devCfg.scl_speed_hz    = m_clkHz;

    i2c_master_dev_handle_t handle = nullptr;
    const esp_err_t err = i2c_master_bus_add_device(m_bus, &devCfg, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "bus_add_device(0x%02x) failed: %d", devAddr, err);
        return nullptr;
    }

    m_devices[m_devCount++] = {devAddr, handle};
    return handle;
}

} // namespace helix
