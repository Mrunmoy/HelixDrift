#include "VirtualI2CBus.hpp"
#include <gtest/gtest.h>
#include <map>

using sim::VirtualI2CBus;
using sim::I2CDevice;
using sim::I2CTransaction;

// Simple mock device for testing
class MockI2CDevice : public I2CDevice {
public:
    struct RegisterState {
        uint8_t value = 0;
        bool writable = true;
    };
    
    std::map<uint8_t, RegisterState> registers;
    bool probed = false;
    
    bool readRegister(uint8_t reg, uint8_t* buf, size_t len) override {
        for (size_t i = 0; i < len; i++) {
            auto it = registers.find(reg + i);
            if (it != registers.end()) {
                buf[i] = it->second.value;
            } else {
                buf[i] = 0x00; // Default for unread registers
            }
        }
        return true;
    }
    
    bool writeRegister(uint8_t reg, const uint8_t* data, size_t len) override {
        for (size_t i = 0; i < len; i++) {
            registers[reg + i].value = data[i];
        }
        return true;
    }
    
    bool probe() override {
        probed = true;
        return true;
    }
};

TEST(VirtualI2CBusTest, CanRegisterAndProbeDevice) {
    VirtualI2CBus bus;
    MockI2CDevice device;
    
    bus.registerDevice(0x6A, device);
    
    EXPECT_TRUE(bus.probe(0x6A));
    EXPECT_TRUE(device.probed);
}

TEST(VirtualI2CBusTest, ProbeFailsForUnregisteredDevice) {
    VirtualI2CBus bus;
    
    EXPECT_FALSE(bus.probe(0x6A));
}

TEST(VirtualI2CBusTest, CanReadFromRegisteredDevice) {
    VirtualI2CBus bus;
    MockI2CDevice device;
    device.registers[0x28] = {0xAB};
    device.registers[0x29] = {0xCD};
    
    bus.registerDevice(0x6A, device);
    
    uint8_t buf[2];
    EXPECT_TRUE(bus.readRegister(0x6A, 0x28, buf, 2));
    EXPECT_EQ(buf[0], 0xAB);
    EXPECT_EQ(buf[1], 0xCD);
}

TEST(VirtualI2CBusTest, ReadFailsForUnregisteredDevice) {
    VirtualI2CBus bus;
    uint8_t buf[1];
    
    EXPECT_FALSE(bus.readRegister(0x6A, 0x00, buf, 1));
}

TEST(VirtualI2CBusTest, CanWriteToRegisteredDevice) {
    VirtualI2CBus bus;
    MockI2CDevice device;
    
    bus.registerDevice(0x6A, device);
    
    uint8_t data[] = {0x12, 0x34};
    EXPECT_TRUE(bus.writeRegister(0x6A, 0x10, data, 2));
    
    EXPECT_EQ(device.registers[0x10].value, 0x12);
    EXPECT_EQ(device.registers[0x11].value, 0x34);
}

TEST(VirtualI2CBusTest, WriteFailsForUnregisteredDevice) {
    VirtualI2CBus bus;
    uint8_t data[] = {0x12};
    
    EXPECT_FALSE(bus.writeRegister(0x6A, 0x00, data, 1));
}

TEST(VirtualI2CBusTest, CanUnregisterDevice) {
    VirtualI2CBus bus;
    MockI2CDevice device;
    
    bus.registerDevice(0x6A, device);
    EXPECT_TRUE(bus.probe(0x6A));
    
    bus.unregisterDevice(0x6A);
    EXPECT_FALSE(bus.probe(0x6A));
}

TEST(VirtualI2CBusTest, LogsTransactions) {
    VirtualI2CBus bus;
    MockI2CDevice device;
    bus.registerDevice(0x6A, device);
    
    // Perform some operations
    bus.probe(0x6A);
    
    uint8_t data[] = {0x42};
    bus.writeRegister(0x6A, 0x10, data, 1);
    
    uint8_t buf[1];
    bus.readRegister(0x6A, 0x20, buf, 1);
    
    // Check transactions were logged
    const auto& txns = bus.getTransactions();
    ASSERT_EQ(txns.size(), 3);
    
    EXPECT_EQ(txns[0].type, I2CTransaction::Type::PROBE);
    EXPECT_EQ(txns[0].devAddr, 0x6A);
    EXPECT_TRUE(txns[0].success);
    
    EXPECT_EQ(txns[1].type, I2CTransaction::Type::WRITE);
    EXPECT_EQ(txns[1].devAddr, 0x6A);
    EXPECT_EQ(txns[1].reg, 0x10);
    ASSERT_EQ(txns[1].data.size(), 1);
    EXPECT_EQ(txns[1].data[0], 0x42);
    
    EXPECT_EQ(txns[2].type, I2CTransaction::Type::READ);
    EXPECT_EQ(txns[2].devAddr, 0x6A);
    EXPECT_EQ(txns[2].reg, 0x20);
}

TEST(VirtualI2CBusTest, CanClearTransactions) {
    VirtualI2CBus bus;
    MockI2CDevice device;
    bus.registerDevice(0x6A, device);
    
    bus.probe(0x6A);
    EXPECT_EQ(bus.getTransactions().size(), 1);
    
    bus.clearTransactions();
    EXPECT_EQ(bus.getTransactions().size(), 0);
}

TEST(VirtualI2CBusTest, CanDisableLogging) {
    VirtualI2CBus bus;
    MockI2CDevice device;
    bus.registerDevice(0x6A, device);
    
    bus.setLoggingEnabled(false);
    bus.probe(0x6A);
    
    EXPECT_EQ(bus.getTransactions().size(), 0);
}

TEST(VirtualI2CBusTest, TransactionCallbackIsInvoked) {
    VirtualI2CBus bus;
    MockI2CDevice device;
    bus.registerDevice(0x6A, device);
    
    I2CTransaction receivedTxn;
    bool callbackCalled = false;
    
    bus.setTransactionCallback([&](const I2CTransaction& txn) {
        receivedTxn = txn;
        callbackCalled = true;
    });
    
    bus.probe(0x6A);
    
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(receivedTxn.type, I2CTransaction::Type::PROBE);
    EXPECT_EQ(receivedTxn.devAddr, 0x6A);
}
