#include "VirtualGimbal.hpp"
#include "Lsm6dsoSimulator.hpp"
#include "Bmm350Simulator.hpp"
#include "Lps22dfSimulator.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace sim {

// Default earth field: 25uT North, 0uT East, -40uT Down
static constexpr float DEFAULT_EARTH_FIELD_X = 25.0f;
static constexpr float DEFAULT_EARTH_FIELD_Y = 0.0f;
static constexpr float DEFAULT_EARTH_FIELD_Z = -40.0f;

VirtualGimbal::VirtualGimbal()
    : orientation_{1.0f, 0.0f, 0.0f, 0.0f},  // Identity quaternion
      rotationRate_{0.0f, 0.0f, 0.0f},
      earthField_{DEFAULT_EARTH_FIELD_X, DEFAULT_EARTH_FIELD_Y, DEFAULT_EARTH_FIELD_Z}
{
}

void VirtualGimbal::setOrientation(const sf::Quaternion& q) {
    orientation_ = q;
    orientation_.normalize();
}

void VirtualGimbal::rotate(const sf::Vec3& axis, float angle) {
    // Normalize the axis
    float len = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (len < 1e-10f) {
        return; // Zero axis, no rotation
    }
    
    // Create rotation quaternion
    float halfAngle = angle * 0.5f;
    float s = std::sin(halfAngle) / len;
    sf::Quaternion rotationQuat{
        std::cos(halfAngle),
        axis.x * s,
        axis.y * s,
        axis.z * s
    };
    
    // Apply rotation: new_orientation = rotation * current
    orientation_ = rotationQuat.multiply(orientation_);
    orientation_.normalize();
}

void VirtualGimbal::reset() {
    orientation_ = {1.0f, 0.0f, 0.0f, 0.0f};
    rotationRate_ = {0.0f, 0.0f, 0.0f};
}

void VirtualGimbal::setRotationRate(float wx, float wy, float wz) {
    rotationRate_ = {wx, wy, wz};
}

void VirtualGimbal::setRotationRate(const sf::Vec3& omega) {
    rotationRate_ = omega;
}

void VirtualGimbal::update(float dt) {
    if (dt <= 0.0f) {
        return;
    }
    
    // Calculate rotation quaternion from angular velocity
    // For small angles: q = [1, omega*dt/2] normalized
    // More accurately: q = [cos(|omega|*dt/2), axis*sin(|omega|*dt/2)]
    
    float omegaMag = std::sqrt(
        rotationRate_.x * rotationRate_.x +
        rotationRate_.y * rotationRate_.y +
        rotationRate_.z * rotationRate_.z
    );
    
    if (omegaMag < 1e-10f) {
        // No rotation
        return;
    }
    
    float halfAngle = omegaMag * dt * 0.5f;
    float sinHalfAngle = std::sin(halfAngle);
    float cosHalfAngle = std::cos(halfAngle);
    
    // Rotation quaternion
    sf::Quaternion rotationQuat{
        cosHalfAngle,
        rotationRate_.x * sinHalfAngle / omegaMag,
        rotationRate_.y * sinHalfAngle / omegaMag,
        rotationRate_.z * sinHalfAngle / omegaMag
    };
    
    // Apply rotation: q_new = rotationQuat * orientation
    orientation_ = rotationQuat.multiply(orientation_);
    orientation_.normalize();
}

void VirtualGimbal::attachAccelGyroSensor(Lsm6dsoSimulator* simulator) {
    if (simulator) {
        accelGyroSensors_.push_back(simulator);
    }
}

void VirtualGimbal::attachMagSensor(Bmm350Simulator* simulator) {
    if (simulator) {
        magSensors_.push_back(simulator);
    }
}

void VirtualGimbal::attachBaroSensor(Lps22dfSimulator* simulator) {
    if (simulator) {
        baroSensors_.push_back(simulator);
    }
}

void VirtualGimbal::syncToSensors() {
    // Sync orientation and rotation rate to all LSM6DSO sensors
    for (auto* sensor : accelGyroSensors_) {
        if (sensor) {
            sensor->setOrientation(orientation_);
            sensor->setRotationRate(rotationRate_.x, rotationRate_.y, rotationRate_.z);
        }
    }
    
    // Sync orientation and earth field to all BMM350 sensors
    for (auto* sensor : magSensors_) {
        if (sensor) {
            sensor->setOrientation(orientation_);
            sensor->setEarthField(earthField_);
        }
    }
    
    // Baro sensors don't need orientation sync, but we keep them registered
    // for potential future altitude-based features
    for (auto* sensor : baroSensors_) {
        (void)sensor; // Currently no-op, but sensor is tracked
    }
}

void VirtualGimbal::setEarthField(const sf::Vec3& field) {
    earthField_ = field;
}

// Simple JSON parser for motion scripts - uses a minimal recursive descent approach
namespace {
    // Skip whitespace
    void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || 
                                      json[pos] == '\n' || json[pos] == '\r')) {
            pos++;
        }
    }
    
    // Parse a float value from JSON (handles both integers and floats)
    bool parseFloat(const std::string& json, size_t& pos, float& outValue) {
        skipWhitespace(json, pos);
        
        size_t start = pos;
        
        // Optional minus sign
        if (pos < json.size() && json[pos] == '-') {
            pos++;
        }
        
        // Integer part
        while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
            pos++;
        }
        
        // Decimal part
        if (pos < json.size() && json[pos] == '.') {
            pos++;
            while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
                pos++;
            }
        }
        
        // Exponent part
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            pos++;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) {
                pos++;
            }
            while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
                pos++;
            }
        }
        
        if (start == pos) {
            return false; // No number found
        }
        
        try {
            outValue = std::stof(json.substr(start, pos - start));
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // Parse a string value from JSON
    bool parseString(const std::string& json, size_t& pos, std::string& outValue) {
        skipWhitespace(json, pos);
        
        if (pos >= json.size() || json[pos] != '"') {
            return false;
        }
        
        pos++; // Skip opening quote
        size_t start = pos;
        
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos += 2; // Skip escaped character
            } else {
                pos++;
            }
        }
        
        if (pos >= json.size()) {
            return false; // No closing quote
        }
        
        outValue = json.substr(start, pos - start);
        pos++; // Skip closing quote
        return true;
    }
    
    // Parse a JSON array of numbers
    bool parseNumberArray(const std::string& json, size_t& pos, std::vector<float>& outValues) {
        skipWhitespace(json, pos);
        
        if (pos >= json.size() || json[pos] != '[') {
            return false;
        }
        
        pos++; // Skip [
        
        while (pos < json.size()) {
            skipWhitespace(json, pos);
            
            if (pos < json.size() && json[pos] == ']') {
                pos++;
                return true;
            }
            
            float val;
            if (parseFloat(json, pos, val)) {
                outValues.push_back(val);
            } else {
                return false;
            }
            
            skipWhitespace(json, pos);
            
            if (pos < json.size() && json[pos] == ',') {
                pos++;
            } else if (pos < json.size() && json[pos] == ']') {
                pos++;
                return true;
            } else {
                return false;
            }
        }
        
        return false;
    }
}

bool VirtualGimbal::loadMotionScript(const std::string& jsonPath) {
    // Read the entire file
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();
    
    // Clear existing script
    motionSteps_.clear();
    
    // Simple parser - find "steps" key and parse array
    size_t stepsPos = json.find("\"steps\"");
    if (stepsPos == std::string::npos) {
        return false;
    }
    
    // Find opening bracket of array
    size_t pos = json.find('[', stepsPos);
    if (pos == std::string::npos) {
        return false;
    }
    pos++; // Skip [
    
    // Parse each step object
    while (pos < json.size()) {
        skipWhitespace(json, pos);
        
        if (pos >= json.size()) {
            break;
        }
        
        // Check for end of array
        if (json[pos] == ']') {
            break;
        }
        
        // Expect object start
        if (json[pos] != '{') {
            return false;
        }
        pos++; // Skip {
        
        MotionStep step;
        
        // Parse object fields
        while (pos < json.size()) {
            skipWhitespace(json, pos);
            
            if (pos < json.size() && json[pos] == '}') {
                pos++; // End of object
                break;
            }
            
            // Parse field name
            std::string fieldName;
            if (!parseString(json, pos, fieldName)) {
                return false;
            }
            
            skipWhitespace(json, pos);
            
            // Expect colon
            if (pos >= json.size() || json[pos] != ':') {
                return false;
            }
            pos++; // Skip :
            
            skipWhitespace(json, pos);
            
            // Parse field value
            if (fieldName == "time") {
                parseFloat(json, pos, step.time);
            }
            else if (fieldName == "action") {
                parseString(json, pos, step.action);
            }
            else if (fieldName == "params") {
                // Parse params object
                if (pos < json.size() && json[pos] == '{') {
                    pos++; // Skip {
                    
                    while (pos < json.size()) {
                        skipWhitespace(json, pos);
                        
                        if (pos < json.size() && json[pos] == '}') {
                            pos++; // End of params
                            break;
                        }
                        
                        // Parse param name
                        std::string paramName;
                        if (!parseString(json, pos, paramName)) {
                            break;
                        }
                        
                        skipWhitespace(json, pos);
                        
                        // Expect colon
                        if (pos < json.size() && json[pos] == ':') {
                            pos++;
                        }
                        
                        skipWhitespace(json, pos);
                        
                        // Parse param value (could be number or array)
                        if (pos < json.size() && json[pos] == '[') {
                            // Array of numbers
                            std::vector<float> values;
                            if (parseNumberArray(json, pos, values)) {
                                for (size_t i = 0; i < values.size(); i++) {
                                    step.params.push_back({paramName + "[" + std::to_string(i) + "]", values[i]});
                                }
                            }
                        } else {
                            // Single number
                            float val;
                            if (parseFloat(json, pos, val)) {
                                step.params.push_back({paramName, val});
                            }
                        }
                        
                        skipWhitespace(json, pos);
                        
                        // Skip comma if present
                        if (pos < json.size() && json[pos] == ',') {
                            pos++;
                        }
                    }
                }
            }
            else {
                // Unknown field - skip it
                if (pos < json.size() && json[pos] == '"') {
                    std::string dummy;
                    parseString(json, pos, dummy);
                } else if (pos < json.size() && (json[pos] == '-' || std::isdigit(static_cast<unsigned char>(json[pos])))) {
                    float dummy;
                    parseFloat(json, pos, dummy);
                } else if (pos < json.size() && json[pos] == '[') {
                    std::vector<float> dummy;
                    parseNumberArray(json, pos, dummy);
                } else if (pos < json.size() && json[pos] == '{') {
                    pos++;
                    int depth = 1;
                    while (pos < json.size() && depth > 0) {
                        if (json[pos] == '{') depth++;
                        else if (json[pos] == '}') depth--;
                        pos++;
                    }
                }
            }
            
            skipWhitespace(json, pos);
            
            // Skip comma if present
            if (pos < json.size() && json[pos] == ',') {
                pos++;
            }
        }
        
        if (!step.action.empty()) {
            motionSteps_.push_back(step);
        }
        
        skipWhitespace(json, pos);
        
        // Skip comma if present
        if (pos < json.size() && json[pos] == ',') {
            pos++;
        }
    }
    
    // Sort steps by time
    std::sort(motionSteps_.begin(), motionSteps_.end(),
              [](const MotionStep& a, const MotionStep& b) {
                  return a.time < b.time;
              });
    
    return true;
}

void VirtualGimbal::runMotionScript() {
    if (motionSteps_.empty()) {
        return;
    }
    
    float currentTime = 0.0f;
    
    for (const auto& step : motionSteps_) {
        // Process each action
        if (step.action == "reset") {
            reset();
        }
        else if (step.action == "setOrientation") {
            float w = 1.0f, x = 0.0f, y = 0.0f, z = 0.0f;
            
            for (const auto& param : step.params) {
                if (param.first == "w") w = param.second;
                else if (param.first == "x") x = param.second;
                else if (param.first == "y") y = param.second;
                else if (param.first == "z") z = param.second;
            }
            
            // Check if axis/angle format
            float axis[3] = {0.0f, 0.0f, 0.0f};
            float angle = 0.0f;
            bool hasAxisAngle = false;
            
            for (const auto& param : step.params) {
                if (param.first == "axis[0]") { axis[0] = param.second; hasAxisAngle = true; }
                else if (param.first == "axis[1]") { axis[1] = param.second; hasAxisAngle = true; }
                else if (param.first == "axis[2]") { axis[2] = param.second; hasAxisAngle = true; }
                else if (param.first == "angle") { angle = param.second; }
            }
            
            if (hasAxisAngle && angle != 0.0f) {
                // Convert from axis/angle (degrees) to quaternion
                // Note: angle in degrees, need to convert to radians
                float angleRad = angle * 3.14159265f / 180.0f;
                setOrientation(sf::Quaternion::fromAxisAngle(axis[0], axis[1], axis[2], angle));
            } else {
                setOrientation({w, x, y, z});
            }
        }
        else if (step.action == "rotate") {
            float axis[3] = {0.0f, 0.0f, 0.0f};
            float angle = 0.0f;
            
            for (const auto& param : step.params) {
                if (param.first == "axis[0]") axis[0] = param.second;
                else if (param.first == "axis[1]") axis[1] = param.second;
                else if (param.first == "axis[2]") axis[2] = param.second;
                else if (param.first == "angle") angle = param.second;
            }
            
            // Angle is in degrees in JSON, convert to radians for rotate()
            float angleRad = angle * 3.14159265f / 180.0f;
            rotate({axis[0], axis[1], axis[2]}, angleRad);
        }
        else if (step.action == "setRotationRate") {
            float wx = 0.0f, wy = 0.0f, wz = 0.0f;
            
            for (const auto& param : step.params) {
                if (param.first == "wx") wx = param.second;
                else if (param.first == "wy") wy = param.second;
                else if (param.first == "wz") wz = param.second;
            }
            
            setRotationRate(wx, wy, wz);
        }
        else if (step.action == "wait") {
            float duration = 0.0f;
            
            for (const auto& param : step.params) {
                if (param.first == "duration") duration = param.second;
            }
            
            if (duration > 0.0f) {
                update(duration);
                currentTime += duration;
            }
        }
        
        // Sync to sensors after each step
        syncToSensors();
    }
}

} // namespace sim
