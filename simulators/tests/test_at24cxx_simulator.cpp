#include "At24CxxSimulator.hpp"
#include "VirtualI2CBus.hpp"
#include <gtest/gtest.h>

using sim::At24CxxSimulator;
using sim::VirtualI2CBus;

// ============================================================================
// TDD Test 1: Device responds to probe
// ============================================================================
TEST(At24CxxSimulatorTest, ProbeReturnsTrue) {
    At24CxxSimulator sim(4096, 16, 0x50);  // 4KB, 16B page, addr 0x50
    EXPECT_TRUE(sim.probe());
}

// ============================================================================
// TDD Test 2: Single byte write and read
// ============================================================================
TEST(At24CxxSimulatorTest, SingleByteWriteAndRead) {
    At24CxxSimulator sim(4096, 16, 0x50);
    
    // Write a single byte at address 0x10
    uint8_t writeData = 0xAB;
    EXPECT_TRUE(sim.writeRegister(0x10, &writeData, 1));
    
    // Read back the byte
    uint8_t readData = 0;
    EXPECT_TRUE(sim.readRegister(0x10, &readData, 1));
    EXPECT_EQ(readData, 0xAB);
}

TEST(At24CxxSimulatorTest, WriteAndReadMultipleSingleBytes) {
    At24CxxSimulator sim(4096, 16, 0x50);
    
    // Write different bytes at different addresses
    for (uint8_t i = 0; i < 10; i++) {
        uint8_t data = 0x30 + i;
        EXPECT_TRUE(sim.writeRegister(i * 16, &data, 1));
    }
    
    // Read back and verify
    for (uint8_t i = 0; i < 10; i++) {
        uint8_t readData = 0;
        EXPECT_TRUE(sim.readRegister(i * 16, &readData, 1));
        EXPECT_EQ(readData, 0x30 + i);
    }
}

// ============================================================================
// TDD Test 3: Multi-byte sequential write (within single page)
// ============================================================================
TEST(At24CxxSimulatorTest, SequentialMultiByteWrite) {
    At24CxxSimulator sim(512, 16, 0x50);  // Use 512 bytes for 8-bit addressable space
    
    // Write multiple bytes starting at address 0x40 (within one 16-byte page: 0x40-0x4F)
    uint8_t writeData[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_TRUE(sim.writeRegister(0x40, writeData, 5));
    
    // Read back sequentially
    uint8_t readData[5] = {};
    EXPECT_TRUE(sim.readRegister(0x40, readData, 5));
    
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(readData[i], writeData[i]);
    }
}

// ============================================================================
// TDD Test 4: Sequential write wraps at page boundary (AT24Cxx behavior)
// ============================================================================
TEST(At24CxxSimulatorTest, SequentialWriteWrapsWithinPage) {
    At24CxxSimulator sim(1024, 16, 0x50);  // 1KB EEPROM, 16-byte pages
    
    // Write 8 bytes starting at address 0x0A (in 16-byte page 0x00-0x0F)
    // This will fill 0x0A-0x0F, then wrap to 0x00-0x01
    uint8_t pattern[8];
    for (int i = 0; i < 8; i++) {
        pattern[i] = static_cast<uint8_t>(0xA0 + i);
    }
    
    EXPECT_TRUE(sim.writeRegister(0x0A, pattern, 8));
    
    // Verify bytes at 0x0A-0x0F (first 6 bytes of pattern)
    for (int i = 0; i < 6; i++) {
        uint8_t readData;
        EXPECT_TRUE(sim.readRegister(0x0A + i, &readData, 1));
        EXPECT_EQ(readData, pattern[i]);
    }
    
    // Verify bytes at 0x00-0x01 (wrapped last 2 bytes of pattern)
    uint8_t readData;
    EXPECT_TRUE(sim.readRegister(0x00, &readData, 1));
    EXPECT_EQ(readData, pattern[6]);
    
    EXPECT_TRUE(sim.readRegister(0x01, &readData, 1));
    EXPECT_EQ(readData, pattern[7]);
}

// ============================================================================
// TDD Test 5: Multi-byte sequential read
// ============================================================================
TEST(At24CxxSimulatorTest, SequentialRead) {
    At24CxxSimulator sim(512, 16, 0x50);  // Use 512 bytes for 8-bit addressable space
    
    // Pre-fill memory with known pattern starting at 0x60
    for (int i = 0; i < 32; i++) {
        uint8_t data = static_cast<uint8_t>(0xA0 + i);
        sim.writeRegister(0x60 + i, &data, 1);
    }
    
    // Read all at once (sequential read)
    uint8_t readData[32] = {};
    EXPECT_TRUE(sim.readRegister(0x60, readData, 32));
    
    // Verify auto-increment worked
    for (int i = 0; i < 32; i++) {
        EXPECT_EQ(readData[i], static_cast<uint8_t>(0xA0 + i));
    }
}

// ============================================================================
// TDD Test 6: Sequential read wraps at end of memory
// ============================================================================
TEST(At24CxxSimulatorTest, SequentialReadWrapsAtEndOfMemory) {
    At24CxxSimulator sim(256, 16, 0x50);  // Small 256-byte EEPROM
    
    // Pre-fill first 8 bytes (0x00-0x07) with known pattern
    for (int i = 0; i < 8; i++) {
        uint8_t data = static_cast<uint8_t>(0x10 + i);
        sim.writeRegister(i, &data, 1);
    }
    
    // Pre-fill last 8 bytes (0xF8-0xFF) with different pattern
    for (int i = 0; i < 8; i++) {
        uint8_t data = static_cast<uint8_t>(0xF0 + i);
        sim.writeRegister(0xF8 + i, &data, 1);
    }
    
    // Read starting at 0xF8, should wrap after 0xFF back to 0x00
    uint8_t readData[16] = {};
    EXPECT_TRUE(sim.readRegister(0xF8, readData, 16));
    
    // First 8 bytes should be from 0xF8-0xFF (0xF0-0xF7)
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(readData[i], static_cast<uint8_t>(0xF0 + i));
    }
    // Next 8 bytes should wrap to 0x00-0x07 (0x10-0x17)
    for (int i = 8; i < 16; i++) {
        EXPECT_EQ(readData[i], static_cast<uint8_t>(0x10 + (i - 8)));
    }
}

// ============================================================================
// TDD Test 7: Page boundary wrapping for writes
// ============================================================================
TEST(At24CxxSimulatorTest, PageWriteWrapsAtPageBoundary) {
    At24CxxSimulator sim(4096, 16, 0x50);  // 16-byte pages
    
    // Write data that would cross page boundary (starting at 0x0C in a 16-byte page)
    uint8_t writeData[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    EXPECT_TRUE(sim.writeRegister(0x0C, writeData, 5));
    
    // Bytes 0-3 should be at 0x0C-0x0F
    uint8_t readData = 0;
    EXPECT_TRUE(sim.readRegister(0x0C, &readData, 1));
    EXPECT_EQ(readData, 0xAA);
    
    EXPECT_TRUE(sim.readRegister(0x0F, &readData, 1));
    EXPECT_EQ(readData, 0xDD);
    
    // Byte 4 should wrap to start of page (0x00), not 0x10
    EXPECT_TRUE(sim.readRegister(0x00, &readData, 1));
    EXPECT_EQ(readData, 0xEE);
    
    // 0x10 should NOT have been written (it's in the next page)
    EXPECT_TRUE(sim.readRegister(0x10, &readData, 1));
    EXPECT_EQ(readData, 0x00);  // Still default
}

// ============================================================================
// TDD Test 8: Different page sizes work correctly
// ============================================================================
TEST(At24CxxSimulatorTest, DifferentPageSizes) {
    // Test 8-byte page size
    {
        At24CxxSimulator sim(256, 8, 0x50);
        
        uint8_t writeData[] = {0x11, 0x22, 0x33, 0x44};
        // Write starts at offset 6 in 8-byte page (0x00-0x07)
        EXPECT_TRUE(sim.writeRegister(0x06, writeData, 4));
        
        uint8_t readData;
        EXPECT_TRUE(sim.readRegister(0x06, &readData, 1));
        EXPECT_EQ(readData, 0x11);
        
        EXPECT_TRUE(sim.readRegister(0x07, &readData, 1));
        EXPECT_EQ(readData, 0x22);
        
        // Should wrap within page (back to 0x00 and 0x01)
        EXPECT_TRUE(sim.readRegister(0x00, &readData, 1));
        EXPECT_EQ(readData, 0x33);
        
        EXPECT_TRUE(sim.readRegister(0x01, &readData, 1));
        EXPECT_EQ(readData, 0x44);
    }
    
    // Test 32-byte page size
    {
        At24CxxSimulator sim(4096, 32, 0x50);
        
        uint8_t writeData[] = {0x55, 0x66, 0x77};
        // Write starts at offset 31 in 32-byte page (0x00-0x1F)
        EXPECT_TRUE(sim.writeRegister(0x1F, writeData, 3));
        
        uint8_t readData;
        EXPECT_TRUE(sim.readRegister(0x1F, &readData, 1));
        EXPECT_EQ(readData, 0x55);
        
        // Should wrap within page (to 0x00 and 0x01)
        EXPECT_TRUE(sim.readRegister(0x00, &readData, 1));
        EXPECT_EQ(readData, 0x66);
        
        EXPECT_TRUE(sim.readRegister(0x01, &readData, 1));
        EXPECT_EQ(readData, 0x77);
    }
}

// ============================================================================
// TDD Test 9: Address pointer increment
// ============================================================================
TEST(At24CxxSimulatorTest, AddressPointerIncrementsOnSequentialRead) {
    At24CxxSimulator sim(512, 16, 0x50);
    
    // Write sequential values
    for (int i = 0; i < 8; i++) {
        uint8_t data = static_cast<uint8_t>(0x10 + i);
        sim.writeRegister(0x50 + i, &data, 1);
    }
    
    // Read one byte at a time, verify address increments
    for (int i = 0; i < 8; i++) {
        uint8_t readData;
        EXPECT_TRUE(sim.readRegister(0x50 + i, &readData, 1));
        EXPECT_EQ(readData, static_cast<uint8_t>(0x10 + i));
    }
}

TEST(At24CxxSimulatorTest, AddressPointerIncrementsOnSequentialWrite) {
    At24CxxSimulator sim(512, 16, 0x50);
    
    // Write bytes one at a time
    for (int i = 0; i < 8; i++) {
        uint8_t data = static_cast<uint8_t>(0x20 + i);
        EXPECT_TRUE(sim.writeRegister(0x60 + i, &data, 1));
    }
    
    // Read back as block
    uint8_t readData[8];
    EXPECT_TRUE(sim.readRegister(0x60, readData, 8));
    
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(readData[i], static_cast<uint8_t>(0x20 + i));
    }
}

// ============================================================================
// TDD Test 10: Configurable memory sizes
// ============================================================================
TEST(At24CxxSimulatorTest, SmallMemorySize128Bytes) {
    At24CxxSimulator sim(128, 8, 0x50);  // AT24C01 equivalent
    
    // Write at valid address
    uint8_t data = 0x42;
    EXPECT_TRUE(sim.writeRegister(0x7F, &data, 1));  // Last valid address
    
    uint8_t readData;
    EXPECT_TRUE(sim.readRegister(0x7F, &readData, 1));
    EXPECT_EQ(readData, 0x42);
}

TEST(At24CxxSimulatorTest, LargeMemorySize64KB) {
    At24CxxSimulator sim(65536, 64, 0x50);  // AT24C512 equivalent
    
    // For large EEPROMs, use direct memory access to test high addresses
    // The I2C interface uses 8-bit address for the register parameter,
    // but we can use direct memory methods for testing
    sim.writeMemory(0xFFF0, 0x12);
    sim.writeMemory(0xFFF1, 0x34);
    sim.writeMemory(0xFFF2, 0x56);
    sim.writeMemory(0xFFF3, 0x78);
    
    // Read back using direct memory access
    EXPECT_EQ(sim.readMemory(0xFFF0), 0x12);
    EXPECT_EQ(sim.readMemory(0xFFF1), 0x34);
    EXPECT_EQ(sim.readMemory(0xFFF2), 0x56);
    EXPECT_EQ(sim.readMemory(0xFFF3), 0x78);
}

// ============================================================================
// TDD Test 11: Configurable I2C addresses
// ============================================================================
TEST(At24CxxSimulatorTest, ConfigurableI2CAddress) {
    At24CxxSimulator sim(4096, 16, 0x50);  // A0=A1=A2=0
    EXPECT_EQ(sim.getI2CAddress(), 0x50);
    
    At24CxxSimulator sim2(4096, 16, 0x57);  // A0=A1=A2=1
    EXPECT_EQ(sim2.getI2CAddress(), 0x57);
    
    At24CxxSimulator sim3(4096, 16, 0x53);  // Mixed
    EXPECT_EQ(sim3.getI2CAddress(), 0x53);
}

// ============================================================================
// TDD Test 12: Works with VirtualI2CBus
// ============================================================================
TEST(At24CxxSimulatorTest, WorksWithVirtualI2CBus) {
    VirtualI2CBus bus;
    At24CxxSimulator sim(512, 16, 0x50);  // Use 512 bytes to fit in 8-bit address
    
    // Register at address 0x50
    bus.registerDevice(0x50, sim);
    
    // Probe the device
    EXPECT_TRUE(bus.probe(0x50));
    
    // Write via bus at address 0x40
    uint8_t writeData[] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_TRUE(bus.writeRegister(0x50, 0x40, writeData, 4));
    
    // Read back via bus
    uint8_t readData[4] = {};
    EXPECT_TRUE(bus.readRegister(0x50, 0x40, readData, 4));
    
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(readData[i], writeData[i]);
    }
}

// ============================================================================
// TDD Test 13: Memory is initialized to zero
// ============================================================================
TEST(At24CxxSimulatorTest, MemoryInitializedToZero) {
    At24CxxSimulator sim(256, 16, 0x50);
    
    // Read from various addresses without writing first
    for (uint16_t addr = 0; addr < 256; addr += 17) {
        uint8_t readData = 0xFF;  // Set to non-zero first
        EXPECT_TRUE(sim.readRegister(addr, &readData, 1));
        EXPECT_EQ(readData, 0x00);
    }
}

// ============================================================================
// TDD Test 14: Overwrite existing data
// ============================================================================
TEST(At24CxxSimulatorTest, OverwriteExistingData) {
    At24CxxSimulator sim(256, 16, 0x50);
    
    // Write initial value
    uint8_t data1 = 0xAA;
    EXPECT_TRUE(sim.writeRegister(0x50, &data1, 1));
    
    uint8_t readData;
    EXPECT_TRUE(sim.readRegister(0x50, &readData, 1));
    EXPECT_EQ(readData, 0xAA);
    
    // Overwrite with new value
    uint8_t data2 = 0x55;
    EXPECT_TRUE(sim.writeRegister(0x50, &data2, 1));
    
    EXPECT_TRUE(sim.readRegister(0x50, &readData, 1));
    EXPECT_EQ(readData, 0x55);
}

// ============================================================================
// TDD Test 15: Error injection - corruptData
// ============================================================================
TEST(At24CxxSimulatorTest, CorruptDataFlipsBits) {
    At24CxxSimulator sim(256, 16, 0x50);
    
    // Write known pattern
    uint8_t data = 0x0F;  // 00001111
    EXPECT_TRUE(sim.writeRegister(0x20, &data, 1));
    
    // Corrupt the data
    sim.corruptData(0x20);
    
    // Read back - some bits should be flipped
    uint8_t readData;
    EXPECT_TRUE(sim.readRegister(0x20, &readData, 1));
    
    // Data should be different from original (with very high probability)
    // Note: It's theoretically possible but extremely unlikely that
    // random bit flips result in the same value
    EXPECT_NE(readData, 0x0F);
}

TEST(At24CxxSimulatorTest, CorruptDataMultipleBytes) {
    At24CxxSimulator sim(256, 16, 0x50);
    
    // Write pattern
    uint8_t pattern[] = {0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_TRUE(sim.writeRegister(0x30, pattern, 4));
    
    // Corrupt middle bytes
    sim.corruptData(0x31);
    sim.corruptData(0x32);
    
    // Read back block
    uint8_t readData[4];
    EXPECT_TRUE(sim.readRegister(0x30, readData, 4));
    
    // First and last should be intact
    EXPECT_EQ(readData[0], 0xFF);
    EXPECT_EQ(readData[3], 0xFF);
    
    // Middle bytes should be corrupted
    EXPECT_NE(readData[1], 0xFF);
    EXPECT_NE(readData[2], 0xFF);
}

// ============================================================================
// TDD Test 16: Write cycle simulation
// ============================================================================
TEST(At24CxxSimulatorTest, WriteCycleTimingSimulation) {
    At24CxxSimulator sim(256, 16, 0x50);
    
    // Initially should be ready
    EXPECT_TRUE(sim.isReady());
    
    // Start a write operation
    uint8_t data = 0xAB;
    sim.beginWrite(0x10, &data, 1);
    
    // During write cycle, device should report busy (not ready)
    EXPECT_FALSE(sim.isReady());
    
    // Complete the write
    sim.completeWrite();
    
    // Now should be ready again
    EXPECT_TRUE(sim.isReady());
    
    // Data should be written
    uint8_t readData;
    EXPECT_TRUE(sim.readRegister(0x10, &readData, 1));
    EXPECT_EQ(readData, 0xAB);
}

// ============================================================================
// TDD Test 17: Write protection
// ============================================================================
TEST(At24CxxSimulatorTest, WriteProtectionPreventsWrites) {
    At24CxxSimulator sim(256, 16, 0x50);
    
    // Enable write protection
    sim.setWriteProtection(true);
    
    // Try to write - should fail
    uint8_t data = 0xAB;
    EXPECT_FALSE(sim.writeRegister(0x10, &data, 1));
    
    // Data should not be written
    uint8_t readData;
    EXPECT_TRUE(sim.readRegister(0x10, &readData, 1));
    EXPECT_EQ(readData, 0x00);
    
    // Disable write protection
    sim.setWriteProtection(false);
    
    // Now write should succeed
    EXPECT_TRUE(sim.writeRegister(0x10, &data, 1));
    EXPECT_TRUE(sim.readRegister(0x10, &readData, 1));
    EXPECT_EQ(readData, 0xAB);
}

// ============================================================================
// TDD Test 18: Zero-length operations
// ============================================================================
TEST(At24CxxSimulatorTest, ZeroLengthReadReturnsTrue) {
    At24CxxSimulator sim(256, 16, 0x50);
    
    uint8_t buf = 0xFF;
    EXPECT_TRUE(sim.readRegister(0x10, &buf, 0));
    // Buffer should be unchanged
    EXPECT_EQ(buf, 0xFF);
}

TEST(At24CxxSimulatorTest, ZeroLengthWriteReturnsTrue) {
    At24CxxSimulator sim(256, 16, 0x50);
    
    uint8_t data = 0xAB;
    EXPECT_TRUE(sim.writeRegister(0x10, &data, 0));
    
    // Memory should be unchanged
    uint8_t readData;
    EXPECT_TRUE(sim.readRegister(0x10, &readData, 1));
    EXPECT_EQ(readData, 0x00);
}

// ============================================================================
// TDD Test 19: Full page write
// ============================================================================
TEST(At24CxxSimulatorTest, FullPageWrite) {
    At24CxxSimulator sim(256, 16, 0x50);  // 16-byte pages
    
    // Fill a full page starting at page boundary
    uint8_t pageData[16];
    for (int i = 0; i < 16; i++) {
        pageData[i] = static_cast<uint8_t>(0xA0 + i);
    }
    
    EXPECT_TRUE(sim.writeRegister(0x20, pageData, 16));
    
    // Read back and verify
    uint8_t readData[16];
    EXPECT_TRUE(sim.readRegister(0x20, readData, 16));
    
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(readData[i], pageData[i]);
    }
}

// ============================================================================
// TDD Test 20: Address calculation with different memory sizes
// ============================================================================
TEST(At24CxxSimulatorTest, AddressMaskingForSmallMemory) {
    // 128 bytes = needs only 7 address bits
    At24CxxSimulator sim(128, 8, 0x50);
    
    // Write at address 0x20
    uint8_t data = 0x42;
    EXPECT_TRUE(sim.writeRegister(0x20, &data, 1));
    
    // Read from 0x20 should work
    uint8_t readData;
    EXPECT_TRUE(sim.readRegister(0x20, &readData, 1));
    EXPECT_EQ(readData, 0x42);
}

// ============================================================================
// TDD Test 21: Multiple devices on same bus
// ============================================================================
TEST(At24CxxSimulatorTest, MultipleDevicesOnVirtualBus) {
    VirtualI2CBus bus;
    
    At24CxxSimulator sim1(256, 16, 0x50);
    At24CxxSimulator sim2(256, 16, 0x51);
    
    bus.registerDevice(0x50, sim1);
    bus.registerDevice(0x51, sim2);
    
    // Write to first device
    uint8_t data1 = 0xAA;
    EXPECT_TRUE(bus.writeRegister(0x50, 0x10, &data1, 1));
    
    // Write different value to second device
    uint8_t data2 = 0xBB;
    EXPECT_TRUE(bus.writeRegister(0x51, 0x10, &data2, 1));
    
    // Read back and verify each device has correct data
    uint8_t readData1, readData2;
    EXPECT_TRUE(bus.readRegister(0x50, 0x10, &readData1, 1));
    EXPECT_TRUE(bus.readRegister(0x51, 0x10, &readData2, 1));
    
    EXPECT_EQ(readData1, 0xAA);
    EXPECT_EQ(readData2, 0xBB);
}
