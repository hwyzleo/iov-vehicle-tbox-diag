#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <string>
#include "data_models.h"

namespace tbox {
namespace diag {

class TransportAdapter {
public:
    virtual ~TransportAdapter() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;

    virtual bool send(const std::vector<uint8_t>& data) = 0;
    virtual std::vector<uint8_t> receive(uint32_t timeout_ms) = 0;

    virtual TransportType get_transport_type() const = 0;
    virtual std::string get_name() const = 0;
};

} // namespace diag
} // namespace tbox
