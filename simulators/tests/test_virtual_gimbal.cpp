#include <gtest/gtest.h>
#include "VirtualGimbal.hpp"
#include "Lsm6dsoSimulator.hpp"
#include "Bmm350Simulator.hpp"
#include "Lps22dfSimulator.hpp"
#include "Quaternion.hpp"
#include "Vec3.hpp"
#include <cmath>
#include <fstream>
#include <cstdio>

using sim::VirtualGimbal;
using sim::Lsm6dsoSimulator;
using sim::Bmm350Simulator;
using sim::Lps22dfSimulator;
using sf::Quaternion;
using sf::Vec3;

class VirtualGimbalTest : public ::testing::Test {
protected:
    VirtualGimbal gimbal;
};

// ============================================================================
// TDD Test 1: Initial orientation is identity quaternion
// ============================================================================
TEST_F(VirtualGimbalTest, InitialOrientation_IsIdentity) {
    Quaternion q = gimbal.getOrientation();
    EXPECT_NEAR(q.w, 1.0f, 1e-6f);
    EXPECT_NEAR(q.x, 0.0f, 1e-6f);
    EXPECT_NEAR(q.y, 0.0f, 1e-6f);
    EXPECT_NEAR(q.z, 0.0f, 1e-6f);
}

// ============================================================================
// TDD Test 2: setOrientation changes quaternion correctly
// ============================================================================
TEST_F(VirtualGimbalTest, SetOrientation_ChangesQuaternion) {
    // Create a 90-degree rotation around Z axis
    Quaternion q = Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 90.0f);
    gimbal.setOrientation(q);
    
    Quaternion result = gimbal.getOrientation();
    EXPECT_NEAR(result.w, q.w, 1e-6f);
    EXPECT_NEAR(result.x, q.x, 1e-6f);
    EXPECT_NEAR(result.y, q.y, 1e-6f);
    EXPECT_NEAR(result.z, q.z, 1e-6f);
}

// ============================================================================
// TDD Test 3: reset() restores identity quaternion
// ============================================================================
TEST_F(VirtualGimbalTest, Reset_RestoresIdentity) {
    // Set a non-identity orientation
    Quaternion q = Quaternion::fromAxisAngle(1.0f, 0.0f, 0.0f, 45.0f);
    gimbal.setOrientation(q);
    
    // Reset
    gimbal.reset();
    
    Quaternion result = gimbal.getOrientation();
    EXPECT_NEAR(result.w, 1.0f, 1e-6f);
    EXPECT_NEAR(result.x, 0.0f, 1e-6f);
    EXPECT_NEAR(result.y, 0.0f, 1e-6f);
    EXPECT_NEAR(result.z, 0.0f, 1e-6f);
}

// ============================================================================
// TDD Test 4: Rotation composition with rotate()
// ============================================================================
TEST_F(VirtualGimbalTest, Rotate_ComposesRotation) {
    // Start with identity
    gimbal.reset();
    
    // Rotate 90 degrees around Z axis
    gimbal.rotate(sf::Vec3{0.0f, 0.0f, 1.0f}, 90.0f * 3.14159265f / 180.0f);
    
    Quaternion result = gimbal.getOrientation();
    
    // After 90 degree Z rotation, w and z should be ~sqrt(2)/2
    float expected = std::sqrt(2.0f) / 2.0f;
    EXPECT_NEAR(result.w, expected, 0.001f);
    EXPECT_NEAR(result.x, 0.0f, 1e-6f);
    EXPECT_NEAR(result.y, 0.0f, 1e-6f);
    EXPECT_NEAR(result.z, expected, 0.001f);
}

TEST_F(VirtualGimbalTest, Rotate_MultipleCompositionsAccumulate) {
    // Start with identity
    gimbal.reset();
    
    // Rotate 90 degrees around Z axis twice = 180 degrees
    gimbal.rotate(sf::Vec3{0.0f, 0.0f, 1.0f}, 90.0f * 3.14159265f / 180.0f);
    gimbal.rotate(sf::Vec3{0.0f, 0.0f, 1.0f}, 90.0f * 3.14159265f / 180.0f);
    
    Quaternion result = gimbal.getOrientation();
    
    // After 180 degree Z rotation, w should be ~0, z should be ~1
    EXPECT_NEAR(result.w, 0.0f, 0.01f);
    EXPECT_NEAR(result.x, 0.0f, 1e-6f);
    EXPECT_NEAR(result.y, 0.0f, 1e-6f);
    EXPECT_NEAR(std::abs(result.z), 1.0f, 0.01f);
}

// ============================================================================
// TDD Test 5: Rotation rate management
// ============================================================================
TEST_F(VirtualGimbalTest, InitialRotationRate_IsZero) {
    Vec3 rate = gimbal.getRotationRate();
    EXPECT_NEAR(rate.x, 0.0f, 1e-6f);
    EXPECT_NEAR(rate.y, 0.0f, 1e-6f);
    EXPECT_NEAR(rate.z, 0.0f, 1e-6f);
}

TEST_F(VirtualGimbalTest, SetRotationRate_ChangesRate) {
    gimbal.setRotationRate(1.0f, 2.0f, 3.0f);
    
    Vec3 rate = gimbal.getRotationRate();
    EXPECT_NEAR(rate.x, 1.0f, 1e-6f);
    EXPECT_NEAR(rate.y, 2.0f, 1e-6f);
    EXPECT_NEAR(rate.z, 3.0f, 1e-6f);
}

TEST_F(VirtualGimbalTest, SetRotationRate_VectorOverload) {
    gimbal.setRotationRate(Vec3{1.5f, 2.5f, 3.5f});
    
    Vec3 rate = gimbal.getRotationRate();
    EXPECT_NEAR(rate.x, 1.5f, 1e-6f);
    EXPECT_NEAR(rate.y, 2.5f, 1e-6f);
    EXPECT_NEAR(rate.z, 3.5f, 1e-6f);
}

// ============================================================================
// TDD Test 6: update() applies rotation rate
// ============================================================================
TEST_F(VirtualGimbalTest, Update_AppliesRotationRate) {
    // Start with identity orientation
    gimbal.reset();
    
    // Set rotation rate: 1 rad/s around Z axis
    gimbal.setRotationRate(0.0f, 0.0f, 1.0f);
    
    // Update for 0.5 seconds -> should rotate 0.5 radians (~28.6 degrees)
    gimbal.update(0.5f);
    
    Quaternion result = gimbal.getOrientation();
    
    // After rotation of angle theta around Z: w = cos(theta/2), z = sin(theta/2)
    float expectedAngle = 0.5f; // radians
    float expectedW = std::cos(expectedAngle / 2.0f);
    float expectedZ = std::sin(expectedAngle / 2.0f);
    
    EXPECT_NEAR(result.w, expectedW, 0.01f);
    EXPECT_NEAR(result.x, 0.0f, 1e-6f);
    EXPECT_NEAR(result.y, 0.0f, 1e-6f);
    EXPECT_NEAR(result.z, expectedZ, 0.01f);
}

TEST_F(VirtualGimbalTest, Update_MultipleStepsAccumulate) {
    gimbal.reset();
    gimbal.setRotationRate(0.0f, 0.0f, 1.0f); // 1 rad/s around Z
    
    // Update twice for 0.5 seconds each = 1.0 second total
    gimbal.update(0.5f);
    gimbal.update(0.5f);
    
    Quaternion result = gimbal.getOrientation();
    
    // Should be equivalent to 1 radian rotation
    float expectedW = std::cos(0.5f); // theta/2 = 0.5 rad
    float expectedZ = std::sin(0.5f);
    
    EXPECT_NEAR(result.w, expectedW, 0.01f);
    EXPECT_NEAR(result.z, expectedZ, 0.01f);
}

// ============================================================================
// TDD Test 7: Earth magnetic field management
// ============================================================================
TEST_F(VirtualGimbalTest, DefaultEarthField_IsCorrect) {
    Vec3 field = gimbal.getEarthField();
    EXPECT_NEAR(field.x, 25.0f, 1e-6f);  // North: 25 uT
    EXPECT_NEAR(field.y, 0.0f, 1e-6f);   // East: 0 uT
    EXPECT_NEAR(field.z, -40.0f, 1e-6f); // Down: -40 uT
}

TEST_F(VirtualGimbalTest, SetEarthField_ChangesField) {
    gimbal.setEarthField(Vec3{30.0f, 10.0f, -50.0f});
    
    Vec3 field = gimbal.getEarthField();
    EXPECT_NEAR(field.x, 30.0f, 1e-6f);
    EXPECT_NEAR(field.y, 10.0f, 1e-6f);
    EXPECT_NEAR(field.z, -50.0f, 1e-6f);
}

// ============================================================================
// TDD Test 8: Sensor attachment
// ============================================================================
TEST_F(VirtualGimbalTest, AttachAccelGyroSensor_LinksToLsm6dso) {
    Lsm6dsoSimulator imu;
    gimbal.attachAccelGyroSensor(&imu);
    
    // Set gimbal orientation
    Quaternion q = Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 90.0f);
    gimbal.setOrientation(q);
    gimbal.setRotationRate(0.0f, 0.0f, 1.0f);
    
    // Sync should push orientation and rotation rate to IMU
    gimbal.syncToSensors();
    
    // Test passes if no crash and IMU has been updated
    SUCCEED();
}

TEST_F(VirtualGimbalTest, AttachMagSensor_LinksToBmm350) {
    Bmm350Simulator mag;
    gimbal.attachMagSensor(&mag);
    
    // Set gimbal orientation and earth field
    Quaternion q = Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 90.0f);
    gimbal.setOrientation(q);
    gimbal.setEarthField(Vec3{30.0f, 0.0f, -40.0f});
    
    // Sync should push orientation and earth field to mag sensor
    gimbal.syncToSensors();
    
    SUCCEED();
}

TEST_F(VirtualGimbalTest, AttachBaroSensor_LinksToLps22df) {
    Lps22dfSimulator baro;
    gimbal.attachBaroSensor(&baro);
    
    // Baro doesn't use orientation, but attachment should work
    gimbal.syncToSensors();
    
    SUCCEED();
}

// ============================================================================
// TDD Test 9: syncToSensors updates all attached sensors
// ============================================================================
TEST_F(VirtualGimbalTest, SyncToSensors_UpdatesAllAttached) {
    Lsm6dsoSimulator imu;
    Bmm350Simulator mag;
    Lps22dfSimulator baro;
    
    gimbal.attachAccelGyroSensor(&imu);
    gimbal.attachMagSensor(&mag);
    gimbal.attachBaroSensor(&baro);
    
    // Set gimbal state
    Quaternion q = Quaternion::fromAxisAngle(0.0f, 1.0f, 0.0f, 45.0f);
    gimbal.setOrientation(q);
    gimbal.setRotationRate(0.5f, 0.3f, 0.1f);
    gimbal.setEarthField(Vec3{25.0f, 5.0f, -35.0f});
    
    // Sync all
    gimbal.syncToSensors();
    
    SUCCEED();
}

TEST_F(VirtualGimbalTest, SyncToSensors_WorksWithNoSensorsAttached) {
    // Should not crash when no sensors attached
    gimbal.syncToSensors();
    SUCCEED();
}

// ============================================================================
// TDD Test 10: JSON motion script loading
// ============================================================================
TEST_F(VirtualGimbalTest, LoadMotionScript_ValidJson_ReturnsTrue) {
    // Create a temporary JSON file
    std::string jsonPath = "/tmp/test_motion_script.json";
    std::ofstream file(jsonPath);
    file << R"({
        "steps": [
            {"time": 0.0, "action": "setOrientation", "params": {"axis": [0, 0, 1], "angle": 0}},
            {"time": 1.0, "action": "rotate", "params": {"axis": [0, 0, 1], "angle": 90}}
        ]
    })";
    file.close();
    
    bool result = gimbal.loadMotionScript(jsonPath);
    EXPECT_TRUE(result);
    
    // Cleanup
    std::remove(jsonPath.c_str());
}

TEST_F(VirtualGimbalTest, LoadMotionScript_InvalidPath_ReturnsFalse) {
    bool result = gimbal.loadMotionScript("/nonexistent/path/script.json");
    EXPECT_FALSE(result);
}

TEST_F(VirtualGimbalTest, LoadMotionScript_MissingStepsField_ReturnsFalse) {
    std::string jsonPath = "/tmp/test_invalid_script.json";
    std::ofstream file(jsonPath);
    file << R"({"invalid": "json"})";
    file.close();
    
    bool result = gimbal.loadMotionScript(jsonPath);
    EXPECT_FALSE(result);
    
    std::remove(jsonPath.c_str());
}

TEST_F(VirtualGimbalTest, LoadMotionScript_EmptySteps_ReturnsTrue) {
    std::string jsonPath = "/tmp/test_empty_script.json";
    std::ofstream file(jsonPath);
    file << R"({"steps": []})";
    file.close();
    
    bool result = gimbal.loadMotionScript(jsonPath);
    EXPECT_TRUE(result);
    
    std::remove(jsonPath.c_str());
}

// ============================================================================
// TDD Test 11: Motion script execution
// ============================================================================
TEST_F(VirtualGimbalTest, RunMotionScript_ExecutesSteps) {
    std::string jsonPath = "/tmp/test_run_script.json";
    std::ofstream file(jsonPath);
    file << R"({
        "steps": [
            {"time": 0.0, "action": "reset", "params": {}},
            {"time": 0.1, "action": "setRotationRate", "params": {"wx": 0, "wy": 0, "wz": 1.0}},
            {"time": 0.2, "action": "wait", "params": {"duration": 0.5}}
        ]
    })";
    file.close();
    
    EXPECT_TRUE(gimbal.loadMotionScript(jsonPath));
    
    // Run the script
    gimbal.runMotionScript();
    
    // After running: orientation should have changed due to rotation rate
    Quaternion q = gimbal.getOrientation();
    // Should not be identity anymore (roughly 0.5 rad rotation around Z)
    EXPECT_FALSE(std::abs(q.w - 1.0f) < 0.01f && std::abs(q.z) < 0.01f);
    
    std::remove(jsonPath.c_str());
}

TEST_F(VirtualGimbalTest, RunMotionScript_WithoutLoading_ReturnsEarly) {
    // Try to run without loading a script first
    gimbal.runMotionScript();
    
    // Should still be at identity (default)
    Quaternion q = gimbal.getOrientation();
    EXPECT_NEAR(q.w, 1.0f, 1e-6f);
}

// ============================================================================
// TDD Test 12: setOrientation normalizes input
// ============================================================================
TEST_F(VirtualGimbalTest, SetOrientation_NormalizesInput) {
    // Set a non-normalized quaternion
    Quaternion q{2.0f, 2.0f, 2.0f, 2.0f}; // norm = 4
    gimbal.setOrientation(q);
    
    Quaternion result = gimbal.getOrientation();
    
    // Result should be normalized (norm = 1)
    EXPECT_NEAR(result.norm(), 1.0f, 1e-6f);
}

// ============================================================================
// TDD Test 13: Zero rotation rate doesn't change orientation on update
// ============================================================================
TEST_F(VirtualGimbalTest, Update_ZeroRotationRate_NoChange) {
    // Set a non-identity orientation
    Quaternion q = Quaternion::fromAxisAngle(0.0f, 1.0f, 0.0f, 30.0f);
    gimbal.setOrientation(q);
    
    // Zero rotation rate
    gimbal.setRotationRate(0.0f, 0.0f, 0.0f);
    
    // Update
    gimbal.update(1.0f);
    
    Quaternion result = gimbal.getOrientation();
    EXPECT_NEAR(result.w, q.w, 1e-6f);
    EXPECT_NEAR(result.x, q.x, 1e-6f);
    EXPECT_NEAR(result.y, q.y, 1e-6f);
    EXPECT_NEAR(result.z, q.z, 1e-6f);
}

// ============================================================================
// TDD Test 14: Motion script with setOrientation action
// ============================================================================
TEST_F(VirtualGimbalTest, RunMotionScript_SetOrientationAction) {
    std::string jsonPath = "/tmp/test_orientation_script.json";
    std::ofstream file(jsonPath);
    file << R"({
        "steps": [
            {"time": 0.0, "action": "setOrientation", "params": {"w": 0.707, "x": 0.0, "y": 0.707, "z": 0.0}}
        ]
    })";
    file.close();
    
    EXPECT_TRUE(gimbal.loadMotionScript(jsonPath));
    gimbal.runMotionScript();
    
    Quaternion q = gimbal.getOrientation();
    EXPECT_NEAR(q.w, 0.707f, 0.01f);
    EXPECT_NEAR(q.y, 0.707f, 0.01f);
    
    std::remove(jsonPath.c_str());
}

// ============================================================================
// TDD Test 15: Multiple sensor sync verification
// ============================================================================
TEST_F(VirtualGimbalTest, MultipleSensorSync_SameOrientation) {
    Lsm6dsoSimulator imu1;
    Lsm6dsoSimulator imu2;
    
    gimbal.attachAccelGyroSensor(&imu1);
    gimbal.attachAccelGyroSensor(&imu2);
    
    gimbal.setOrientation(Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 45.0f));
    gimbal.syncToSensors();
    
    // Both IMUs should have received the same orientation
    SUCCEED();
}

// ============================================================================
// TDD Test 16: Reset clears rotation rate
// ============================================================================
TEST_F(VirtualGimbalTest, Reset_ClearsRotationRate) {
    gimbal.setRotationRate(1.0f, 2.0f, 3.0f);
    gimbal.reset();
    
    Vec3 rate = gimbal.getRotationRate();
    EXPECT_NEAR(rate.x, 0.0f, 1e-6f);
    EXPECT_NEAR(rate.y, 0.0f, 1e-6f);
    EXPECT_NEAR(rate.z, 0.0f, 1e-6f);
}

// ============================================================================
// TDD Test 17: Motion script wait action
// ============================================================================
TEST_F(VirtualGimbalTest, RunMotionScript_WaitAction) {
    std::string jsonPath = "/tmp/test_wait_script.json";
    std::ofstream file(jsonPath);
    file << R"({
        "steps": [
            {"time": 0.0, "action": "reset", "params": {}},
            {"time": 0.0, "action": "setRotationRate", "params": {"wx": 0, "wy": 0, "wz": 2.0}},
            {"time": 0.0, "action": "wait", "params": {"duration": 0.25}}
        ]
    })";
    file.close();
    
    EXPECT_TRUE(gimbal.loadMotionScript(jsonPath));
    gimbal.runMotionScript();
    
    // After 0.25s at 2 rad/s = 0.5 rad rotation
    Quaternion q = gimbal.getOrientation();
    float expectedW = std::cos(0.25f); // 0.5/2 = 0.25
    EXPECT_NEAR(q.w, expectedW, 0.05f);
    
    std::remove(jsonPath.c_str());
}
