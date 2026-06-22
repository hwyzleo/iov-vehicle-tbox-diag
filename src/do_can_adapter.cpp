#include "do_can_adapter.h"
#include <iostream>

namespace tbox {
namespace diag {

DoCanAdapter::DoCanAdapter(const DoCanConfig& config) : config_(config) {}

bool DoCanAdapter::connect() {
    // TODO: Implement actual CAN ISO-TP connection
    // 1. Open CAN socket on config_.can_interface
    // 2. Bind to rx_id
    connected_ = true;
    return true;
}

void DoCanAdapter::disconnect() {
    // TODO: Implement actual CAN disconnect
    connected_ = false;
}

bool DoCanAdapter::is_connected() const {
    return connected_;
}

bool DoCanAdapter::send(const std::vector<uint8_t>& data) {
    if (!connected_) {
        return false;
    }
    // TODO: Implement actual ISO-TP send
    // Segmentation if needed (single frame / multi frame)
    return true;
}

std::vector<uint8_t> DoCanAdapter::receive(uint32_t timeout_ms) {
    if (!connected_) {
        return {};
    }
    // TODO: Implement actual ISO-TP receive
    // Reassemble multi-frame if needed
    return {};
}

TransportType DoCanAdapter::get_transport_type() const {
    return TransportType::DO_CAN;
}

std::string DoCanAdapter::get_name() const {
    return "DoCAN(" + config_.can_interface + " tx:0x" +
           std::to_string(config_.tx_id) + " rx:0x" +
           std::to_string(config_.rx_id) + ")";
}

} // namespace diag
} // namespace tbox
