#pragma once

#include <cstdint>

namespace helix {

struct MocapNodeLoopConfig {
    uint8_t nodeId = 1;
    uint32_t outputPeriodUs = 20000;
};

struct NoopSampleHook {
    template <typename SampleT>
    void onSample(uint64_t, SampleT&) const {}
};

template <typename Clock,
          typename Pipeline,
          typename Transport,
          typename Sample,
          typename SampleHook = NoopSampleHook>
class MocapNodeLoopT {
public:
    MocapNodeLoopT(Clock& clock,
                   Pipeline& pipeline,
                   Transport& transport,
                   const MocapNodeLoopConfig& cfg,
                   SampleHook sampleHook = {})
        : clock_(clock),
          pipeline_(pipeline),
          transport_(transport),
          cfg_(cfg),
          sampleHook_(sampleHook)
    {}

    bool tick() {
        const uint64_t nowUs = clock_.nowUs();
        if (!isDue(nowUs)) {
            return false;
        }
        nextTickUs_ = nowUs + cfg_.outputPeriodUs;

        Sample sample{};
        if (!pipeline_.step(sample)) {
            return false;
        }
        sampleHook_.onSample(nowUs, sample);
        return transport_.sendQuaternion(cfg_.nodeId, nowUs, sample.orientation);
    }

private:
    bool isDue(uint64_t nowUs) {
        if (!initialized_) {
            initialized_ = true;
            nextTickUs_ = nowUs;
        }
        return nowUs >= nextTickUs_;
    }

    Clock& clock_;
    Pipeline& pipeline_;
    Transport& transport_;
    MocapNodeLoopConfig cfg_{};
    SampleHook sampleHook_{};
    uint64_t nextTickUs_ = 0;
    bool initialized_ = false;
};

} // namespace helix
