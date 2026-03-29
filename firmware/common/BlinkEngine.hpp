#pragma once

#include <cstdint>

namespace helix {

class BlinkEngine {
public:
    BlinkEngine(uint32_t onMs, uint32_t offMs, bool startOn = true);

    void reset();
    bool level() const { return level_; }
    bool tick(uint32_t elapsedMs);

private:
    uint32_t onMs_;
    uint32_t offMs_;
    bool startOn_;
    bool level_;
    uint32_t phaseRemainingMs_;

    uint32_t activePhaseDuration() const;
    void switchPhase();
};

} // namespace helix
