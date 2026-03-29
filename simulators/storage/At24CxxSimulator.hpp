#pragma once

#include "VirtualI2CBus.hpp"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <random>

namespace sim {

/**
 * @brief AT24Cxx EEPROM Simulator
 * 
 * Simulates AT24Cxx series I2C EEPROMs with:
 * - Configurable memory size (128 bytes to 64KB)
 * - Configurable page size (8, 16, 32, or 64 bytes)
 * - Configurable I2C address (0x50-0x57)
 * - Page write boundary handling (address wraps within page)
 * - Sequential read/write with address auto-increment
 * - Memory address pointer management
 * - Write cycle timing simulation
 * - Write protection simulation
 * - Error injection (data corruption)
 * 
 * AT24Cxx Protocol:
 * - Device address: 0x50-0x57 (configurable via A0-A2 pins)
 * - Write: [DeviceAddr+W] [MemAddr] [Data0] [Data1] ...
 * - Read: [DeviceAddr+W] [MemAddr] [DeviceAddr+R] [Data0] [Data1] ...
 * - Sequential read: address auto-increments, wraps at end of memory
 * - Page write: address wraps at page boundary within same page
 */
class At24CxxSimulator : public I2CDevice {
public:
    /**
     * @brief Construct an AT24Cxx EEPROM simulator
     * 
     * @param memorySize Total memory size in bytes (128 to 65536)
     * @param pageSize Page size in bytes (8, 16, 32, or 64)
     * @param i2cAddress I2C device address (0x50-0x57)
     * @param writeCycleMs Simulated write cycle time in milliseconds (default 5ms)
     */
    At24CxxSimulator(size_t memorySize = 4096, 
                     size_t pageSize = 16, 
                     uint8_t i2cAddress = 0x50,
                     uint32_t writeCycleMs = 5);
    
    ~At24CxxSimulator() override = default;
    
    // Disable copy/move
    At24CxxSimulator(const At24CxxSimulator&) = delete;
    At24CxxSimulator& operator=(const At24CxxSimulator&) = delete;
    
    // I2CDevice interface implementation
    bool probe() override;
    bool readRegister(uint8_t reg, uint8_t* buf, size_t len) override;
    bool writeRegister(uint8_t reg, const uint8_t* data, size_t len) override;
    
    /**
     * @brief Get the configured I2C address
     */
    uint8_t getI2CAddress() const { return i2cAddress_; }
    
    /**
     * @brief Get the total memory capacity
     */
    size_t getMemorySize() const { return memorySize_; }
    
    /**
     * @brief Get the page size
     */
    size_t getPageSize() const { return pageSize_; }
    
    /**
     * @brief Direct memory access for testing/verification
     * @param address Memory address to read
     * @return Byte value at address
     */
    uint8_t readMemory(uint16_t address) const;
    
    /**
     * @brief Direct memory write for testing
     * @param address Memory address to write
     * @param data Byte value to write
     */
    void writeMemory(uint16_t address, uint8_t data);
    
    /**
     * @brief Enable/disable write protection (simulates WP pin)
     * @param enabled true to enable write protection
     */
    void setWriteProtection(bool enabled) { writeProtected_ = enabled; }
    
    /**
     * @brief Check if write protection is enabled
     */
    bool isWriteProtected() const { return writeProtected_; }
    
    /**
     * @brief Corrupt data at a specific address (error injection)
     * Flips random bits in the data at the specified address.
     * @param address Memory address to corrupt
     */
    void corruptData(uint16_t address);
    
    /**
     * @brief Check if device is ready (write cycle complete)
     */
    bool isReady() const { return !writeInProgress_; }
    
    /**
     * @brief Begin a write operation (simulates write cycle timing)
     * 
     * This initiates a write operation. The device will be "busy"
     * until completeWrite() is called.
     * 
     * @param address Starting memory address
     * @param data Data buffer
     * @param len Number of bytes to write
     */
    void beginWrite(uint16_t address, const uint8_t* data, size_t len);
    
    /**
     * @brief Complete the write operation
     * 
     * Call this after beginWrite() to complete the write cycle.
     * In a real implementation, this would be called automatically
     * after writeCycleMs milliseconds.
     */
    void completeWrite();
    
    /**
     * @brief Reset the device to initial state
     * Clears all memory to 0, resets address pointer.
     */
    void reset();

private:
    // Configuration
    size_t memorySize_;
    size_t pageSize_;
    uint8_t i2cAddress_;
    uint32_t writeCycleMs_;
    
    // Memory storage
    std::unique_ptr<uint8_t[]> memory_;
    
    // State
    uint16_t addressPointer_ = 0;  // Current address for sequential access
    bool writeProtected_ = false;
    bool writeInProgress_ = false;
    
    // Write cycle buffer
    uint16_t writeAddress_ = 0;
    std::vector<uint8_t> writeBuffer_;
    
    // Random number generator for error injection
    mutable std::mt19937 rng_;
    
    // Helper methods
    uint16_t calculateAddress(uint8_t reg) const;
    uint16_t maskAddress(uint16_t address) const;
    void writeToMemory(uint16_t address, const uint8_t* data, size_t len);
    void readFromMemory(uint16_t address, uint8_t* buf, size_t len) const;
    uint16_t getPageStart(uint16_t address) const;
    uint16_t getPageOffset(uint16_t address) const;
};

} // namespace sim
