#pragma once

#include <cstdint>

namespace helix {

// Stable target identifiers for OTA begin/status checks.
constexpr uint32_t kOtaTargetIdNrf52dkNrf52832 = 0x52832001u;
constexpr uint32_t kOtaTargetIdNrf52840Dongle  = 0x52840059u;
constexpr uint32_t kOtaTargetIdXiaoNrf52840    = 0x52840040u;

} // namespace helix
