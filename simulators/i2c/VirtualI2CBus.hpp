#pragma once

#include "II2CBus.hpp"
#include <cstdint>
#include <cstddef>
#include <functional>
#include <map>
#include <vector>

namespace sim {

/**
 * @brief Simulated I2C device interface
 * 
 * Any device that wants to be addressable on the virtual I2C bus
 * must implement this interface.
 */
class I2CDevice {
public:
    virtual ~I2CDevice() = default;
    
    /**
     * @brief Handle a register read from the device
     * @param reg Register address
     * @param buf Output buffer
     * @param len Number of bytes to read
     * @return true if read was successful
     */
    virtual bool readRegister(uint8_t reg, uint8_t* buf, size_t len) = 0;
    
    /**
     * @brief Handle a register write to the device
     * @param reg Register address  
     * @param data Input data
     * @param len Number of bytes to write
     * @return true if write was successful
     */
    virtual bool writeRegister(uint8_t reg, const uint8_t* data, size_t len) = 0;
    
    /**
     * @brief Handle probe (just check if device responds)
     * @return true if device is present
     */
    virtual bool probe() = 0;
};

/**
 * @brief I2C transaction log entry
 */
struct I2CTransaction {
    enum class Type { READ, WRITE, PROBE };
    
    Type type;
    uint8_t devAddr;
    uint8_t reg;
    std::vector<uint8_t> data;
    bool success;
    uint64_t timestampUs;
};

/**
 * @brief Virtual I2C bus that routes transactions to simulated devices
 * 
 * This implements the sf::II2CBus interface, allowing real sensor drivers
 * to communicate with simulated sensor devices.
 */
class VirtualI2CBus : public sf::II2CBus {
public:
    using TransactionCallback = std::function<void(const I2CTransaction&)>;
    
    VirtualI2CBus() = default;
    ~VirtualI2CBus() override = default;
    
    // Disable copy/move
    VirtualI2CBus(const VirtualI2CBus&) = delete;
    VirtualI2CBus& operator=(const VirtualI2CBus&) = delete;
    
    /**
     * @brief Register a device at a specific I2C address
     */
    void registerDevice(uint8_t address, I2CDevice& device);
    
    /**
     * @brief Unregister a device
     */
    void unregisterDevice(uint8_t address);
    
    /**
     * @brief Set callback for transaction logging
     */
    void setTransactionCallback(TransactionCallback cb);
    
    /**
     * @brief Get logged transactions (if logging enabled)
     */
    const std::vector<I2CTransaction>& getTransactions() const { return transactions_; }
    
    /**
     * @brief Clear transaction log
     */
    void clearTransactions() { transactions_.clear(); }
    
    /**
     * @brief Enable/disable transaction logging
     */
    void setLoggingEnabled(bool enabled) { loggingEnabled_ = enabled; }
    
    // sf::II2CBus implementation
    bool readRegister(uint8_t devAddr, uint8_t reg, uint8_t* buf, size_t len) override;
    bool writeRegister(uint8_t devAddr, uint8_t reg, const uint8_t* data, size_t len) override;
    bool probe(uint8_t devAddr) override;
    
private:
    std::map<uint8_t, I2CDevice*> devices_;
    std::vector<I2CTransaction> transactions_;
    TransactionCallback transactionCb_;
    bool loggingEnabled_ = true;
    
    void logTransaction(const I2CTransaction& txn);
    I2CDevice* findDevice(uint8_t address);
};

} // namespace sim
