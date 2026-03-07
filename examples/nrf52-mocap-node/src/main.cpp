#include "BMM350.hpp"
#include "LSM6DSO.hpp"
#include "LPS22DF.hpp"
#include "MocapBleTransport.hpp"
#include "MocapCalibrationFlow.hpp"
#include "MocapHealthTelemetry.hpp"
#include "MocapNodePipeline.hpp"
#include "MocapBleSender.hpp"
#include "MocapNodeLoop.hpp"
#include "MocapProfiles.hpp"
#include "TimestampSync.hpp"
#include "TimestampSynchronizedTransport.hpp"
#include "NrfDelay.hpp"
#include "NrfTwimBus.hpp"
#include "board_xiao_nrf52840.hpp"

namespace {
constexpr uint8_t kNodeId = 1;
constexpr helix::MocapPowerMode kPowerMode = helix::MocapPowerMode::PERFORMANCE;

struct NrfDelayClock {
    sf::NrfDelay& delay;
    uint64_t nowUs() const { return delay.getTimestampUs(); }
};

struct CalibrationHook {
    sf::MocapCalibrationFlow& flow;

    void onSample(uint64_t, sf::MocapNodeSample& sample) {
        const uint8_t cmdRaw = sf_mocap_calibration_command();
        if (cmdRaw <= static_cast<uint8_t>(sf::MocapCalibrationFlow::Command::Reset)) {
            const auto cmd = static_cast<sf::MocapCalibrationFlow::Command>(cmdRaw);
            if (cmd != sf::MocapCalibrationFlow::Command::None) {
                flow.issue(cmd);
            }
        }
        (void)flow.processSample(sample.orientation);
        sample.orientation = flow.apply(sample.orientation);
    }
};

struct SyncAnchorSource {
    bool poll(uint64_t& localUs, uint64_t& remoteUs) {
        return sf_mocap_sync_anchor(&localUs, &remoteUs);
    }
};
} // namespace

int main() {
    constexpr helix::MocapProfile profile = helix::selectMocapProfile(kPowerMode);

    if (!xiao_board_init_i2c()) {
        while (true) {}
    }

    sf::NrfTwimBus imuBus(g_twim0);
    sf::NrfTwimBus envBus(g_twim1);
    sf::NrfDelay delay;

    sf::LSM6DSOConfig imuCfg{};
    imuCfg.accelOdr = (profile.imuOdrHz >= 200) ? sf::LsmOdr::HZ_208 : sf::LsmOdr::HZ_104;
    imuCfg.gyroOdr = (profile.imuOdrHz >= 200) ? sf::LsmOdr::HZ_208 : sf::LsmOdr::HZ_104;
    sf::LSM6DSO imu(imuBus, delay, imuCfg);

    sf::BMM350Config magCfg{};
    magCfg.odr = (profile.magOdrHz >= 100) ? sf::Bmm350Odr::HZ_100 : sf::Bmm350Odr::HZ_50;
    sf::BMM350 mag(envBus, delay, magCfg);

    sf::LPS22DFConfig baroCfg{};
    baroCfg.odr = (profile.baroOdrHz >= 200) ? sf::LpsOdr::HZ_200 : sf::LpsOdr::HZ_25;
    sf::LPS22DF baro(envBus, delay, baroCfg);

    if (!imu.init() || !mag.init() || !baro.init()) {
        while (true) delay.delayMs(1000);
    }

    sf::MocapNodePipeline::Config nodeCfg{};
    nodeCfg.dtSeconds = profile.dtSeconds;
    nodeCfg.preferMag = true;
    sf::MocapNodePipeline pipeline(imu, &mag, &baro, nodeCfg);

    sf::MocapBleTransport::Config bleCfg{};
    bleCfg.attMtu = 185;
    bleCfg.maxRetries = 2;
    helix::WeakSymbolBleSender bleSender;
    using BaseBleTransport = sf::MocapBleTransportT<helix::BleSenderAdapter>;
    BaseBleTransport bleTx(helix::BleSenderAdapter(&bleSender), bleCfg);
    sf::TimestampSync timestampSync{};
    SyncAnchorSource syncAnchorSource{};
    helix::TimestampSynchronizedTransportT<BaseBleTransport, sf::TimestampSync, SyncAnchorSource>
        syncedTx(bleTx, timestampSync, syncAnchorSource);
    helix::NodeHealthTelemetryEmitterT<helix::BleSenderAdapter> healthTx{
        helix::BleSenderAdapter(&bleSender)};
    sf::MocapCalibrationFlow calibrationFlow{};
    CalibrationHook calibrationHook{calibrationFlow};
    NrfDelayClock clock{delay};
    helix::MocapNodeLoopConfig loopCfg{};
    loopCfg.nodeId = kNodeId;
    loopCfg.outputPeriodUs = profile.outputPeriodUs;
    helix::MocapNodeLoopT<
        NrfDelayClock,
        sf::MocapNodePipeline,
        helix::TimestampSynchronizedTransportT<BaseBleTransport, sf::TimestampSync, SyncAnchorSource>,
        sf::MocapNodeSample,
        CalibrationHook> loop(clock, pipeline, syncedTx, loopCfg, calibrationHook);

    uint64_t nextHealthTickUs = clock.nowUs();
    while (true) {
        const uint64_t nowUs = clock.nowUs();
        if (nowUs >= nextHealthTickUs) {
            nextHealthTickUs = nowUs + 1000000ULL;
            uint16_t batteryMv = 0;
            uint8_t batteryPercent = 0;
            uint8_t linkQuality = 0;
            uint16_t droppedFrames = 0;
            uint8_t flags = 0;
            if (sf_mocap_health_sample(
                    &batteryMv, &batteryPercent, &linkQuality, &droppedFrames, &flags)) {
                helix::NodeHealthTelemetry telemetry{};
                telemetry.batteryMv = batteryMv;
                telemetry.batteryPercent = batteryPercent;
                telemetry.linkQuality = linkQuality;
                telemetry.droppedFrames = droppedFrames;
                telemetry.calibrationState = static_cast<uint8_t>(calibrationFlow.state());
                telemetry.flags = flags;
                (void)healthTx.send(
                    kNodeId,
                    timestampSync.toRemoteTimeUs(nowUs),
                    telemetry);
            }
        }
        if (!loop.tick()) {
            delay.delayMs(1);
        }
    }
}
