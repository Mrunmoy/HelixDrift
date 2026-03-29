#include "CsvExport.hpp"
#include "VirtualMocapNodeHarness.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace sim;

namespace {

class ScopedExportEnv {
public:
    explicit ScopedExportEnv(const char* value) {
        const char* current = std::getenv("HELIX_TEST_EXPORT");
        if (current != nullptr) {
            hadOriginal_ = true;
            originalValue_ = current;
        }

        if (value != nullptr) {
            setenv("HELIX_TEST_EXPORT", value, 1);
        } else {
            unsetenv("HELIX_TEST_EXPORT");
        }
    }

    ~ScopedExportEnv() {
        if (hadOriginal_) {
            setenv("HELIX_TEST_EXPORT", originalValue_.c_str(), 1);
        } else {
            unsetenv("HELIX_TEST_EXPORT");
        }
    }

private:
    bool hadOriginal_ = false;
    std::string originalValue_;
};

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace

TEST(CsvExportTest, WritesHeaderAndRowsForCapturedRun) {
    VirtualMocapNodeHarness harness;
    ASSERT_TRUE(harness.initAll());
    harness.resetAndSync();

    const NodeRunResult result = harness.runForDuration(40000, 20000);
    ASSERT_EQ(result.samples.size(), 2u);

    const std::string path = "csv_export_test_output.csv";
    std::remove(path.c_str());

    ASSERT_TRUE(exportCsv(result, path));

    const std::string csv = readFile(path);
    EXPECT_NE(csv.find("time_s,truth_w,truth_x"), std::string::npos);
    EXPECT_NE(csv.find("pressure_hpa,error_deg"), std::string::npos);

    std::istringstream lines(csv);
    std::string line;
    int lineCount = 0;
    while (std::getline(lines, line)) {
        ++lineCount;
    }
    EXPECT_EQ(lineCount, 3);

    std::remove(path.c_str());
}

TEST(CsvExportTest, ExportIfEnabledSkipsWriteWhenEnvVarIsUnset) {
    ScopedExportEnv env(nullptr);

    NodeRunResult result{};
    result.samples.push_back(CapturedNodeSample{});
    const std::string path = "csv_export_disabled.csv";
    std::remove(path.c_str());

    EXPECT_FALSE(exportCsvIfEnabled(result, path));

    std::ifstream in(path);
    EXPECT_FALSE(in.good());
}

TEST(CsvExportTest, ExportIfEnabledWritesWhenEnvVarIsSetToOne) {
    ScopedExportEnv env("1");

    NodeRunResult result{};
    CapturedNodeSample sample{};
    sample.timestampUs = 20000;
    sample.pressureHPa = 1013.25f;
    sample.angularErrorDeg = 1.5f;
    result.samples.push_back(sample);

    const std::string path = "csv_export_enabled.csv";
    std::remove(path.c_str());

    ASSERT_TRUE(exportCsvIfEnabled(result, path));

    const std::string csv = readFile(path);
    EXPECT_NE(csv.find("1013.250000"), std::string::npos);
    EXPECT_NE(csv.find("1.500000"), std::string::npos);

    std::remove(path.c_str());
}

TEST(CsvExportTest, DefaultCsvPathUsesTestOutputDirectoryAndSanitizedStem) {
    const std::string path = defaultCsvPath("CsvExportTest.Path Uses Spaces/Slashes");

    EXPECT_NE(path.find("test_output/"), std::string::npos);
    EXPECT_NE(path.find("CsvExportTest_Path_Uses_Spaces_Slashes.csv"), std::string::npos);
    EXPECT_TRUE(std::filesystem::exists("test_output"));
}

TEST(CsvExportTest, DefaultAnalysisRunDirUsesExperimentsRunsDirectory) {
    const std::string path = defaultAnalysisRunDir("CsvExportTest.Analysis Run");

    EXPECT_NE(path.find("experiments/runs/"), std::string::npos);
    EXPECT_NE(path.find("CsvExportTest_Analysis_Run"), std::string::npos);
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(path).parent_path()));
}

TEST(CsvExportTest, ExportAnalysisRunWritesManifestAndSchemaCompatibleSamples) {
    NodeRunResult result{};
    CapturedNodeSample sample{};
    sample.timestampUs = 20000;
    sample.truthOrientation = sf::Quaternion::fromAxisAngle(0.0f, 0.0f, 1.0f, 15.0f);
    sample.fusedOrientation = sample.truthOrientation;
    sample.angularErrorDeg = 0.25f;
    result.samples.push_back(sample);

    AnalysisRunManifest manifest{};
    manifest.experimentId = "csv_export_analysis";
    manifest.description = "contract test";
    manifest.initialOrientation = sample.truthOrientation;
    manifest.measuredSamples = 1;
    manifest.totalTicks = 1;
    manifest.durationSeconds = 0.02f;

    const std::filesystem::path runDir("analysis_export_test_output");
    std::filesystem::remove_all(runDir);

    ASSERT_TRUE(exportAnalysisRun(result, manifest, runDir.string()));

    const std::string manifestJson = readFile((runDir / "manifest.json").string());
    const std::string samplesCsv = readFile((runDir / "samples.csv").string());

    EXPECT_NE(manifestJson.find("\"experiment_id\": \"csv_export_analysis\""), std::string::npos);
    EXPECT_NE(samplesCsv.find("sample_idx,timestamp_us,truth_w"), std::string::npos);
    EXPECT_NE(samplesCsv.find("angular_error_deg"), std::string::npos);
    EXPECT_NE(samplesCsv.find("0,20000,"), std::string::npos);

    std::filesystem::remove_all(runDir);
}

TEST(CsvExportTest, ExportAnalysisRunIfEnabledSkipsWriteWhenEnvVarUnset) {
    ScopedExportEnv env(nullptr);

    NodeRunResult result{};
    result.samples.push_back(CapturedNodeSample{});

    AnalysisRunManifest manifest{};
    manifest.experimentId = "disabled_analysis";
    manifest.initialOrientation = sf::Quaternion{};

    const std::filesystem::path runDir("analysis_export_disabled");
    std::filesystem::remove_all(runDir);

    EXPECT_FALSE(exportAnalysisRunIfEnabled(result, manifest, runDir.string()));
    EXPECT_FALSE(std::filesystem::exists(runDir));
}
