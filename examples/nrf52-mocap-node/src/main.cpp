#include "BMM350.hpp"
#include "LSM6DSO.hpp"
#include "LPS22DF.hpp"
#include "MocapBleTransport.hpp"
#include "MocapCalibrationFlow.hpp"
#include "MocapNodePipeline.hpp"
#include "MocapBleSender.hpp"
#include "MocapNodeLoop.hpp"
#include "MocapProfiles.hpp"
#include "TimestampSync.hpp"
#include "TimestampSynchronizedTransport.hpp"
#include "NrfDelay.hpp"
#include "NrfTwimBus.hpp"

extern const nrfx_twim_t g_twim0 = {};
extern const nrfx_twim_t g_twim1 = {};
extern "C" uint8_t __attribute__((weak)) sf_mocap_calibration_command() { return 0; }
extern "C" bool __attribute__((weak)) sf_mocap_sync_anchor(uint64_t* localUs, uint64_t* remoteUs) {
    (void)localUs;
    (void)remoteUs;
    return false;
}

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

    while (true) {
        if (!loop.tick()) {
            delay.delayMs(1);
        }
    }
}
