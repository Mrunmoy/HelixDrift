#include "VirtualI2CBus.hpp"
#include <chrono>

namespace sim {

void VirtualI2CBus::registerDevice(uint8_t address, I2CDevice& device) {
    devices_[address] = &device;
}

void VirtualI2CBus::unregisterDevice(uint8_t address) {
    devices_.erase(address);
}

void VirtualI2CBus::setTransactionCallback(TransactionCallback cb) {
    transactionCb_ = std::move(cb);
}

I2CDevice* VirtualI2CBus::findDevice(uint8_t address) {
    auto it = devices_.find(address);
    if (it != devices_.end()) {
        return it->second;
    }
    return nullptr;
}

void VirtualI2CBus::logTransaction(const I2CTransaction& txn) {
    if (loggingEnabled_) {
        transactions_.push_back(txn);
    }
    if (transactionCb_) {
        transactionCb_(txn);
    }
}

bool VirtualI2CBus::readRegister(uint8_t devAddr, uint8_t reg, uint8_t* buf, size_t len) {
    I2CDevice* device = findDevice(devAddr);
    
    I2CTransaction txn;
    txn.type = I2CTransaction::Type::READ;
    txn.devAddr = devAddr;
    txn.reg = reg;
    txn.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (!device) {
        txn.success = false;
        logTransaction(txn);
        return false;
    }
    
    txn.success = device->readRegister(reg, buf, len);
    if (txn.success && buf && len > 0) {
        txn.data.assign(buf, buf + len);
    }
    
    logTransaction(txn);
    return txn.success;
}

bool VirtualI2CBus::writeRegister(uint8_t devAddr, uint8_t reg, const uint8_t* data, size_t len) {
    I2CDevice* device = findDevice(devAddr);
    
    I2CTransaction txn;
    txn.type = I2CTransaction::Type::WRITE;
    txn.devAddr = devAddr;
    txn.reg = reg;
    txn.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (data && len > 0) {
        txn.data.assign(data, data + len);
    }
    
    if (!device) {
        txn.success = false;
        logTransaction(txn);
        return false;
    }
    
    txn.success = device->writeRegister(reg, data, len);
    logTransaction(txn);
    return txn.success;
}

bool VirtualI2CBus::probe(uint8_t devAddr) {
    I2CDevice* device = findDevice(devAddr);
    
    I2CTransaction txn;
    txn.type = I2CTransaction::Type::PROBE;
    txn.devAddr = devAddr;
    txn.reg = 0;
    txn.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (!device) {
        txn.success = false;
        logTransaction(txn);
        return false;
    }
    
    txn.success = device->probe();
    logTransaction(txn);
    return txn.success;
}

} // namespace sim
