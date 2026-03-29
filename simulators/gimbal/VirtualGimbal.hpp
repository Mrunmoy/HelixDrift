#pragma once

#include "Quaternion.hpp"
#include "Vec3.hpp"
#include <string>
#include <vector>
#include <memory>

// Forward declarations for sensor simulators
namespace sim {
class Lsm6dsoSimulator;
class Bmm350Simulator;
class Lps22dfSimulator;

/**
 * @brief Virtual Gimbal for sensor simulation
 * 
 * Manages the orientation and motion of a virtual sensor block, coordinating
 * multiple sensor simulators (LSM6DSO, BMM350, LPS22DF) with a unified
 * kinematic model. Supports motion scripts for automated testing scenarios.
 */
class VirtualGimbal {
public:
    /**
     * @brief Construct a new Virtual Gimbal with identity orientation
     */
    VirtualGimbal();
    
    /**
     * @brief Destroy the Virtual Gimbal
     */
    ~VirtualGimbal() = default;
    
    // Disable copy/move
    VirtualGimbal(const VirtualGimbal&) = delete;
    VirtualGimbal& operator=(const VirtualGimbal&) = delete;
    VirtualGimbal(VirtualGimbal&&) = default;
    VirtualGimbal& operator=(VirtualGimbal&&) = default;

    // ========================================================================
    // Orientation management
    // ========================================================================
    
    /**
     * @brief Set the absolute orientation
     * @param q Quaternion representing the new orientation (will be normalized)
     */
    void setOrientation(const sf::Quaternion& q);
    
    /**
     * @brief Get the current orientation
     * @return Current orientation quaternion
     */
    sf::Quaternion getOrientation() const { return orientation_; }
    
    /**
     * @brief Apply a relative rotation to the current orientation
     * @param axis Rotation axis (will be normalized)
     * @param angle Rotation angle in radians
     */
    void rotate(const sf::Vec3& axis, float angle);
    
    /**
     * @brief Reset orientation to identity and clear rotation rate
     */
    void reset();

    // ========================================================================
    // Rotation rate
    // ========================================================================
    
    /**
     * @brief Set the rotation rate (angular velocity)
     * @param wx Angular velocity around X axis in rad/s
     * @param wy Angular velocity around Y axis in rad/s
     * @param wz Angular velocity around Z axis in rad/s
     */
    void setRotationRate(float wx, float wy, float wz);
    
    /**
     * @brief Set the rotation rate using a vector
     * @param omega Angular velocity vector in rad/s
     */
    void setRotationRate(const sf::Vec3& omega);
    
    /**
     * @brief Get the current rotation rate
     * @return Current angular velocity vector in rad/s
     */
    sf::Vec3 getRotationRate() const { return rotationRate_; }
    
    /**
     * @brief Update orientation based on current rotation rate over time
     * @param dt Time delta in seconds
     * 
     * Applies the rotation rate to update the orientation using the formula:
     * q_new = q_current * quaternion_from_axis_angle(omega * dt)
     */
    void update(float dt);

    // ========================================================================
    // Sensor coordination
    // ========================================================================
    
    /**
     * @brief Attach an accelerometer/gyroscope simulator (LSM6DSO)
     * @param simulator Pointer to the LSM6DSO simulator
     */
    void attachAccelGyroSensor(Lsm6dsoSimulator* simulator);
    
    /**
     * @brief Attach a magnetometer simulator (BMM350)
     * @param simulator Pointer to the BMM350 simulator
     */
    void attachMagSensor(Bmm350Simulator* simulator);
    
    /**
     * @brief Attach a barometer simulator (LPS22DF)
     * @param simulator Pointer to the LPS22DF simulator
     */
    void attachBaroSensor(Lps22dfSimulator* simulator);
    
    /**
     * @brief Synchronize orientation and earth field to all attached sensors
     * 
     * This pushes the current gimbal state to all registered sensor simulators,
     * ensuring they generate data consistent with the virtual orientation.
     */
    void syncToSensors();

    // ========================================================================
    // Motion scripts (JSON)
    // ========================================================================
    
    /**
     * @brief Load a motion script from a JSON file
     * @param jsonPath Path to the JSON motion script file
     * @return true if the script was loaded successfully
     * 
     * JSON format:
     * {
     *   "steps": [
     *     {"time": 0.0, "action": "setOrientation", "params": {...}},
     *     {"time": 1.0, "action": "rotate", "params": {...}},
     *     {"time": 2.0, "action": "setRotationRate", "params": {...}},
     *     {"time": 3.0, "action": "wait", "params": {"duration": 1.0}},
     *     {"time": 4.0, "action": "reset", "params": {}}
     *   ]
     * }
     */
    bool loadMotionScript(const std::string& jsonPath);
    
    /**
     * @brief Run the loaded motion script
     * 
     * Executes all steps in the loaded motion script in chronological order.
     * Steps are sorted by time before execution.
     */
    void runMotionScript();
    
    /**
     * @brief Check if a motion script is loaded
     * @return true if a script is loaded and ready to run
     */
    bool hasMotionScript() const { return !motionSteps_.empty(); }
    
    /**
     * @brief Clear the loaded motion script
     */
    void clearMotionScript() { motionSteps_.clear(); }

    // ========================================================================
    // Earth magnetic field
    // ========================================================================
    
    /**
     * @brief Set the local earth magnetic field vector
     * @param field Magnetic field vector in microtesla (uT)
     * 
     * Default value is {25.0, 0.0, -40.0} representing:
     * - X (North): 25 uT
     * - Y (East): 0 uT  
     * - Z (Down): -40 uT
     */
    void setEarthField(const sf::Vec3& field);
    
    /**
     * @brief Get the current earth magnetic field vector
     * @return Magnetic field vector in microtesla (uT)
     */
    sf::Vec3 getEarthField() const { return earthField_; }

private:
    // Current orientation
    sf::Quaternion orientation_;
    
    // Current rotation rate in rad/s
    sf::Vec3 rotationRate_;
    
    // Earth magnetic field in uT (default: North 25uT, East 0uT, Down -40uT)
    sf::Vec3 earthField_;
    
    // Attached sensors
    std::vector<Lsm6dsoSimulator*> accelGyroSensors_;
    std::vector<Bmm350Simulator*> magSensors_;
    std::vector<Lps22dfSimulator*> baroSensors_;
    
    // Motion script representation
    struct MotionStep {
        float time = 0.0f;
        std::string action;
        std::vector<std::pair<std::string, float>> params;
    };
    std::vector<MotionStep> motionSteps_;
    
    // Helper methods
    sf::Quaternion quaternionFromAngularVelocity(const sf::Vec3& omega, float dt);
};

} // namespace sim
