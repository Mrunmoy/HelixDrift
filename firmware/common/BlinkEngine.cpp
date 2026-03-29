#include "BlinkEngine.hpp"

namespace helix {

BlinkEngine::BlinkEngine(uint32_t onMs, uint32_t offMs, bool startOn)
    : onMs_(onMs),
      offMs_(offMs),
      startOn_(startOn),
      level_(startOn),
      phaseRemainingMs_(startOn ? onMs : offMs)
{}

void BlinkEngine::reset() {
    level_ = startOn_;
    phaseRemainingMs_ = startOn_ ? onMs_ : offMs_;
}

uint32_t BlinkEngine::activePhaseDuration() const {
    return level_ ? onMs_ : offMs_;
}

void BlinkEngine::switchPhase() {
    level_ = !level_;
    phaseRemainingMs_ = activePhaseDuration();
}

bool BlinkEngine::tick(uint32_t elapsedMs) {
    if (onMs_ == 0 && offMs_ == 0) return level_;
    if (elapsedMs == 0) return level_;

    uint32_t remaining = elapsedMs;
    while (remaining > 0) {
        if (phaseRemainingMs_ == 0) {
            switchPhase();
            if (phaseRemainingMs_ == 0) {
                return level_;
            }
        }

        if (remaining < phaseRemainingMs_) {
            phaseRemainingMs_ -= remaining;
            remaining = 0;
        } else {
            remaining -= phaseRemainingMs_;
            phaseRemainingMs_ = 0;
            switchPhase();
        }
    }

    return level_;
}

} // namespace helix
