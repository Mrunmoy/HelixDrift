#pragma once

#include <cstdint>

namespace helix {

template <typename Transport, typename SyncFilter, typename AnchorSource>
class TimestampSynchronizedTransportT {
public:
    TimestampSynchronizedTransportT(Transport& transport,
                                    SyncFilter& syncFilter,
                                    AnchorSource& anchorSource)
        : transport_(transport),
          syncFilter_(syncFilter),
          anchorSource_(anchorSource)
    {}

    template <typename QuaternionT>
    bool sendQuaternion(uint8_t nodeId, uint64_t localTimestampUs, const QuaternionT& q) {
        uint64_t anchorLocalUs = 0;
        uint64_t anchorRemoteUs = 0;
        if (anchorSource_.poll(anchorLocalUs, anchorRemoteUs)) {
            syncFilter_.observeAnchor(anchorLocalUs, anchorRemoteUs);
        }
        const uint64_t mappedTimestampUs = syncFilter_.toRemoteTimeUs(localTimestampUs);
        return transport_.sendQuaternion(nodeId, mappedTimestampUs, q);
    }

private:
    Transport& transport_;
    SyncFilter& syncFilter_;
    AnchorSource& anchorSource_;
};

} // namespace helix
