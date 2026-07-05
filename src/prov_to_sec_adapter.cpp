#include "prov_to_sec_adapter.h"
#include <iostream>

namespace tbox {
namespace diag {

ProvToSecAdapter::ProvToSecAdapter(std::shared_ptr<prov::ProvClient> client)
    : client_(std::move(client)) {}

sec::ErrorCode ProvToSecAdapter::initialize() {
    if (!client_) {
        return sec::ErrorCode::NOT_INITIALIZED;
    }
    return sec::ErrorCode::SUCCESS;
}

sec::ErrorCode ProvToSecAdapter::get_vehicle_info(sec::VehicleInfo& info) {
    if (!client_ || !client_->is_connected()) {
        return sec::ErrorCode::NOT_INITIALIZED;
    }

    auto binding = client_->read_binding();
    info.vin = binding.vin;
    info.ecu_uid = binding.ecu_uid;
    return sec::ErrorCode::SUCCESS;
}

bool ProvToSecAdapter::is_connected() const {
    return client_ && client_->is_connected();
}

std::string ProvToSecAdapter::get_service_status() const {
    if (!client_) {
        return "NOT_INITIALIZED";
    }
    return client_->is_connected() ? "CONNECTED" : "DISCONNECTED";
}

} // namespace diag
} // namespace tbox
