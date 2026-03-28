#include "At24CxxSimulator.hpp"
#include <cstring>
#include <algorithm>
#include <chrono>

namespace sim {

At24CxxSimulator::At24CxxSimulator(size_t memorySize, 
                                   size_t pageSize, 
                                   uint8_t i2cAddress,
                                   uint32_t writeCycleMs)
    : memorySize_(memorySize)
    , pageSize_(pageSize)
    , i2cAddress_(i2cAddress)
    , writeCycleMs_(writeCycleMs)
    , addressPointer_(0)
    , writeProtected_(false)
    , writeInProgress_(false)
    , writeAddress_(0)
    , rng_(std::chrono::steady_clock::now().time_since_epoch().count())
{
    // Validate parameters
    if (memorySize_ < 128) memorySize_ = 128;
    if (memorySize_ > 65536) memorySize_ = 65536;
    
    // Page size must be power of 2 and reasonable
    if (pageSize_ != 8 && pageSize_ != 16 && pageSize_ != 32 && pageSize_ != 64) {
        pageSize_ = 16;  // Default
    }
    
    // I2C address must be in range 0x50-0x57
    if (i2cAddress_ < 0x50 || i2cAddress_ > 0x57) {
        i2cAddress_ = 0x50;
    }
    
    // Allocate memory and initialize to zero
    memory_ = std::make_unique<uint8_t[]>(memorySize_);
    std::memset(memory_.get(), 0, memorySize_);
}

bool At24CxxSimulator::probe() {
    // Device is always present and responds to probe
    return true;
}

bool At24CxxSimulator::readRegister(uint8_t reg, uint8_t* buf, size_t len) {
    if (!buf) {
        return false;
    }
    
    // Zero-length read is valid (no-op)
    if (len == 0) {
        return true;
    }
    
    // Calculate starting address from register value
    uint16_t address = calculateAddress(reg);
    
    // Perform sequential read with address auto-increment and memory wrap
    for (size_t i = 0; i < len; i++) {
        uint16_t currentAddr = maskAddress(address + static_cast<uint16_t>(i));
        buf[i] = memory_[currentAddr];
    }
    
    // Update address pointer to next address after read
    addressPointer_ = maskAddress(address + static_cast<uint16_t>(len));
    
    return true;
}

bool At24CxxSimulator::writeRegister(uint8_t reg, const uint8_t* data, size_t len) {
    if (!data) {
        return false;
    }
    
    // Zero-length write is valid (no-op)
    if (len == 0) {
        return true;
    }
    
    // Check write protection
    if (writeProtected_) {
        return false;
    }
    
    // Calculate starting address
    uint16_t startAddress = calculateAddress(reg);
    
    // Perform write with page boundary handling
    // In AT24Cxx, page writes wrap within the page boundary
    for (size_t i = 0; i < len; i++) {
        // Calculate page-aware address
        uint16_t pageStart = getPageStart(startAddress);
        uint16_t pageOffset = getPageOffset(startAddress + static_cast<uint16_t>(i));
        uint16_t currentAddr = pageStart + pageOffset;
        
        memory_[currentAddr] = data[i];
    }
    
    // Update address pointer
    addressPointer_ = startAddress;
    
    return true;
}

uint8_t At24CxxSimulator::readMemory(uint16_t address) const {
    return memory_[maskAddress(address)];
}

void At24CxxSimulator::writeMemory(uint16_t address, uint8_t data) {
    if (!writeProtected_) {
        memory_[maskAddress(address)] = data;
    }
}

void At24CxxSimulator::corruptData(uint16_t address) {
    uint16_t addr = maskAddress(address);
    
    // Generate random bit mask (randomly flip 1-4 bits)
    std::uniform_int_distribution<int> bitCountDist(1, 4);
    std::uniform_int_distribution<int> bitPosDist(0, 7);
    
    uint8_t flipMask = 0;
    int bitsToFlip = bitCountDist(rng_);
    
    for (int i = 0; i < bitsToFlip; i++) {
        flipMask |= (1 << bitPosDist(rng_));
    }
    
    memory_[addr] ^= flipMask;
}

void At24CxxSimulator::beginWrite(uint16_t address, const uint8_t* data, size_t len) {
    if (writeProtected_ || !data || len == 0) {
        return;
    }
    
    // Store write parameters
    writeAddress_ = maskAddress(address);
    writeBuffer_.assign(data, data + len);
    writeInProgress_ = true;
}

void At24CxxSimulator::completeWrite() {
    if (!writeInProgress_) {
        return;
    }
    
    // Perform the actual write with page boundary handling
    for (size_t i = 0; i < writeBuffer_.size(); i++) {
        uint16_t pageStart = getPageStart(writeAddress_);
        uint16_t pageOffset = getPageOffset(writeAddress_ + static_cast<uint16_t>(i));
        uint16_t currentAddr = pageStart + pageOffset;
        
        memory_[currentAddr] = writeBuffer_[i];
    }
    
    writeInProgress_ = false;
    writeBuffer_.clear();
}

void At24CxxSimulator::reset() {
    std::memset(memory_.get(), 0, memorySize_);
    addressPointer_ = 0;
    writeInProgress_ = false;
    writeBuffer_.clear();
}

uint16_t At24CxxSimulator::calculateAddress(uint8_t reg) const {
    // For AT24Cxx, the 'reg' parameter is the memory address byte
    // For larger EEPROMs (>256 bytes), the address is typically
    // sent as part of the device address or as multiple bytes
    // In this simulation, we use the reg as the low byte of the address
    // and combine with any previous address pointer state if needed
    
    // For simplicity in this interface, 'reg' is the direct memory address
    // for EEPROMs <= 256 bytes, or the lower 8 bits for larger EEPROMs
    return maskAddress(static_cast<uint16_t>(reg));
}

uint16_t At24CxxSimulator::maskAddress(uint16_t address) const {
    // Mask address to fit within memory size
    // Handle non-power-of-2 memory sizes properly
    // Note: memorySize_ is size_t (can be up to 65536), need to handle overflow
    if (memorySize_ >= 65536) {
        // For 64KB or larger, all 16-bit addresses are valid
        return address;
    }
    return address % static_cast<uint16_t>(memorySize_);
}

uint16_t At24CxxSimulator::getPageStart(uint16_t address) const {
    // Get the start address of the page containing this address
    return (address / static_cast<uint16_t>(pageSize_)) * static_cast<uint16_t>(pageSize_);
}

uint16_t At24CxxSimulator::getPageOffset(uint16_t address) const {
    // Get the offset within the page, wrapping at page boundary
    return address % static_cast<uint16_t>(pageSize_);
}

void At24CxxSimulator::writeToMemory(uint16_t address, const uint8_t* data, size_t len) {
    if (!data || writeProtected_) {
        return;
    }
    
    for (size_t i = 0; i < len; i++) {
        memory_[maskAddress(address + i)] = data[i];
    }
}

void At24CxxSimulator::readFromMemory(uint16_t address, uint8_t* buf, size_t len) const {
    if (!buf) {
        return;
    }
    
    for (size_t i = 0; i < len; i++) {
        buf[i] = memory_[maskAddress(address + i)];
    }
}

} // namespace sim
