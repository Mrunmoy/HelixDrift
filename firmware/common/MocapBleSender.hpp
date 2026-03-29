#pragma once

#include <cstddef>
#include <cstdint>

namespace helix {

class BleSender {
public:
    virtual ~BleSender() = default;
    virtual bool send(const uint8_t* data, size_t len) = 0;
};

class WeakSymbolBleSender final : public BleSender {
public:
    bool send(const uint8_t* data, size_t len) override;
};

class BleSenderAdapter {
public:
    explicit BleSenderAdapter(BleSender* sender)
        : sender_(sender)
    {}

    bool valid() const { return sender_ != nullptr; }

    bool operator()(const uint8_t* data, size_t len) const {
        return sender_ != nullptr && sender_->send(data, len);
    }

private:
    BleSender* sender_;
};

} // namespace helix
