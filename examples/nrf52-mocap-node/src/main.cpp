#include "BMM350.hpp"
#include "LSM6DSO.hpp"
#include "LPS22DF.hpp"
#include "MocapBleTransport.hpp"
#include "MocapNodePipeline.hpp"
#include "MocapProfiles.hpp"
#include "NrfDelay.hpp"
#include "NrfTwimBus.hpp"

extern "C" bool __attribute__((weak)) sf_mocap_ble_notify(const uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    // Implement in board-specific BLE code.
    return false;
}

extern const nrfx_twim_t g_twim0 = {};
extern const nrfx_twim_t g_twim1 = {};

namespace {
constexpr uint8_t kNodeId = 1;
constexpr helix::MocapPowerMode kPowerMode = helix::MocapPowerMode::PERFORMANCE;

bool bleNotifyAdapter(const uint8_t* data, size_t len, void* context) {
    (void)context;
    return sf_mocap_ble_notify(data, len);
}
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
    sf::MocapBleTransport bleTx(&bleNotifyAdapter, nullptr, bleCfg);

    uint64_t nextTickUs = delay.getTimestampUs();
    while (true) {
        const uint64_t nowUs = delay.getTimestampUs();
        if (nowUs < nextTickUs) {
            delay.delayMs(1);
            continue;
        }
        nextTickUs = nowUs + profile.outputPeriodUs;

        sf::MocapNodeSample sample{};
        if (!pipeline.step(sample)) continue;
        (void)bleTx.sendQuaternion(kNodeId, nowUs, sample.orientation);
    }
}
