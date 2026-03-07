#include "MocapBleSender.hpp"

extern "C" bool __attribute__((weak)) sf_mocap_ble_notify(const uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    return false;
}

namespace helix {

bool WeakSymbolBleSender::send(const uint8_t* data, size_t len) {
    return sf_mocap_ble_notify(data, len);
}

} // namespace helix
