#pragma once

#include "VirtualMocapNodeHarness.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <string>

namespace sim {

inline bool shouldExport() {
    const char* value = std::getenv("HELIX_TEST_EXPORT");
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

inline bool exportCsv(const NodeRunResult& result, const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }

    out << "time_s,truth_w,truth_x,truth_y,truth_z,"
           "fused_w,fused_x,fused_y,fused_z,"
           "accel_x,accel_y,accel_z,"
           "gyro_x,gyro_y,gyro_z,"
           "mag_x,mag_y,mag_z,"
           "pressure_hpa,error_deg\n";
    out << std::fixed << std::setprecision(6);

    for (const auto& sample : result.samples) {
        const double timeS = static_cast<double>(sample.timestampUs) / 1000000.0;
        out << timeS << ','
            << sample.truthOrientation.w << ',' << sample.truthOrientation.x << ','
            << sample.truthOrientation.y << ',' << sample.truthOrientation.z << ','
            << sample.fusedOrientation.w << ',' << sample.fusedOrientation.x << ','
            << sample.fusedOrientation.y << ',' << sample.fusedOrientation.z << ','
            << sample.rawAccel.x << ',' << sample.rawAccel.y << ',' << sample.rawAccel.z << ','
            << sample.rawGyro.x << ',' << sample.rawGyro.y << ',' << sample.rawGyro.z << ','
            << sample.rawMag.x << ',' << sample.rawMag.y << ',' << sample.rawMag.z << ','
            << sample.pressureHPa << ',' << sample.angularErrorDeg << '\n';
    }

    return out.good();
}

inline bool exportCsvIfEnabled(const NodeRunResult& result, const std::string& path) {
    if (!shouldExport()) {
        return false;
    }
    return exportCsv(result, path);
}

} // namespace sim
