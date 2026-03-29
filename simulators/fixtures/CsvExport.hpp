#pragma once

#include "VirtualMocapNodeHarness.hpp"

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace sim {

struct AnalysisRunManifest {
    std::string experimentId;
    std::string experimentFamily = "Wave_B";
    std::string description;
    sf::Quaternion initialOrientation{};
    sf::Vec3 rotationRateDps{0.0f, 0.0f, 0.0f};
    uint32_t warmupSamples = 0;
    uint32_t measuredSamples = 0;
    uint32_t totalTicks = 0;
    float durationSeconds = 0.0f;
    uint32_t seed = 42;
    uint32_t outputPeriodUs = 20000;
    float mahonyKp = 1.0f;
    float mahonyKi = 0.0f;
    sf::Vec3 gyroBiasRadS{0.0f, 0.0f, 0.0f};
    float gyroNoiseStd = 0.0f;
    sf::Vec3 accelBiasG{0.0f, 0.0f, 0.0f};
    sf::Vec3 magHardIronUt{0.0f, 0.0f, 0.0f};
};

inline bool shouldExport() {
    const char* value = std::getenv("HELIX_TEST_EXPORT");
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

inline std::string sanitizeCsvStem(const std::string& stem) {
    std::string sanitized = stem;
    for (char& ch : sanitized) {
        const bool ok = (ch >= 'a' && ch <= 'z') ||
                        (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') ||
                        ch == '_' || ch == '-';
        if (!ok) {
            ch = '_';
        }
    }
    return sanitized;
}

inline std::string defaultCsvPath(const std::string& testName) {
    const std::filesystem::path outDir("test_output");
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    const std::string stem = sanitizeCsvStem(testName);
    return (outDir / (stem + ".csv")).string();
}

inline std::string defaultAnalysisRunDir(const std::string& testName) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTm{};
#if defined(_WIN32)
    localtime_s(&localTm, &nowTime);
#else
    localtime_r(&nowTime, &localTm);
#endif
    std::ostringstream dateStream;
    dateStream << std::put_time(&localTm, "%Y%m%d");

    const std::filesystem::path outDir =
        std::filesystem::path("experiments") / "runs" / dateStream.str() / sanitizeCsvStem(testName);
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    return outDir.string();
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

inline bool exportAnalysisRun(const NodeRunResult& result,
                              const AnalysisRunManifest& manifest,
                              const std::string& runDir) {
    const std::filesystem::path outDir(runDir);
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec) {
        return false;
    }

    std::ofstream manifestOut(outDir / "manifest.json");
    if (!manifestOut.is_open()) {
        return false;
    }

    manifestOut << "{\n"
                << "  \"experiment_id\": \"" << manifest.experimentId << "\",\n"
                << "  \"experiment_family\": \"" << manifest.experimentFamily << "\",\n"
                << "  \"description\": \"" << manifest.description << "\",\n"
                << "  \"simulator_config\": {\n"
                << "    \"mahony_kp\": " << manifest.mahonyKp << ",\n"
                << "    \"mahony_ki\": " << manifest.mahonyKi << ",\n"
                << "    \"seed\": " << manifest.seed << ",\n"
                << "    \"output_period_us\": " << manifest.outputPeriodUs << "\n"
                << "  },\n"
                << "  \"gimbal_config\": {\n"
                << "    \"initial_orientation\": {\n"
                << "      \"w\": " << manifest.initialOrientation.w << ",\n"
                << "      \"x\": " << manifest.initialOrientation.x << ",\n"
                << "      \"y\": " << manifest.initialOrientation.y << ",\n"
                << "      \"z\": " << manifest.initialOrientation.z << "\n"
                << "    },\n"
                << "    \"rotation_rate_dps\": {\n"
                << "      \"x\": " << manifest.rotationRateDps.x << ",\n"
                << "      \"y\": " << manifest.rotationRateDps.y << ",\n"
                << "      \"z\": " << manifest.rotationRateDps.z << "\n"
                << "    },\n"
                << "    \"motion_profile\": \"exported_run\"\n"
                << "  },\n"
                << "  \"sensor_errors\": {\n"
                << "    \"gyro_bias_rad_s\": {\n"
                << "      \"x\": " << manifest.gyroBiasRadS.x << ",\n"
                << "      \"y\": " << manifest.gyroBiasRadS.y << ",\n"
                << "      \"z\": " << manifest.gyroBiasRadS.z << "\n"
                << "    },\n"
                << "    \"gyro_noise_std\": " << manifest.gyroNoiseStd << ",\n"
                << "    \"accel_bias_g\": {\n"
                << "      \"x\": " << manifest.accelBiasG.x << ",\n"
                << "      \"y\": " << manifest.accelBiasG.y << ",\n"
                << "      \"z\": " << manifest.accelBiasG.z << "\n"
                << "    },\n"
                << "    \"mag_hard_iron_ut\": {\n"
                << "      \"x\": " << manifest.magHardIronUt.x << ",\n"
                << "      \"y\": " << manifest.magHardIronUt.y << ",\n"
                << "      \"z\": " << manifest.magHardIronUt.z << "\n"
                << "    }\n"
                << "  },\n"
                << "  \"execution\": {\n"
                << "    \"warmup_samples\": " << manifest.warmupSamples << ",\n"
                << "    \"measured_samples\": " << manifest.measuredSamples << ",\n"
                << "    \"total_ticks\": " << manifest.totalTicks << ",\n"
                << "    \"duration_seconds\": " << manifest.durationSeconds << "\n"
                << "  }\n"
                << "}\n";
    if (!manifestOut.good()) {
        return false;
    }

    std::ofstream samplesOut(outDir / "samples.csv");
    if (!samplesOut.is_open()) {
        return false;
    }

    samplesOut << "sample_idx,timestamp_us,"
                  "truth_w,truth_x,truth_y,truth_z,"
                  "fused_w,fused_x,fused_y,fused_z,"
                  "angular_error_deg\n";
    samplesOut << std::fixed << std::setprecision(6);
    for (std::size_t i = 0; i < result.samples.size(); ++i) {
        const auto& sample = result.samples[i];
        samplesOut << i << ','
                   << sample.timestampUs << ','
                   << sample.truthOrientation.w << ',' << sample.truthOrientation.x << ','
                   << sample.truthOrientation.y << ',' << sample.truthOrientation.z << ','
                   << sample.fusedOrientation.w << ',' << sample.fusedOrientation.x << ','
                   << sample.fusedOrientation.y << ',' << sample.fusedOrientation.z << ','
                   << sample.angularErrorDeg << '\n';
    }

    return samplesOut.good();
}

inline bool exportAnalysisRunIfEnabled(const NodeRunResult& result,
                                       const AnalysisRunManifest& manifest,
                                       const std::string& runDir) {
    if (!shouldExport()) {
        return false;
    }
    return exportAnalysisRun(result, manifest, runDir);
}

} // namespace sim
