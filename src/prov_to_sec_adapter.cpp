#include "prov_to_sec_adapter.h"
#include <iostream>

namespace tbox {
namespace diag {

ProvToSecAdapter::ProvToSecAdapter(std::shared_ptr<prov::ProvService> service)
    : service_(std::move(service)) {}

sec::ErrorCode ProvToSecAdapter::initialize() {
    if (!service_) {
        return sec::ErrorCode::NOT_INITIALIZED;
    }
    return sec::ErrorCode::SUCCESS;
}

sec::ErrorCode ProvToSecAdapter::get_vehicle_info(sec::VehicleInfo& info) {
    if (!service_ || !service_->is_initialized()) {
        return sec::ErrorCode::NOT_INITIALIZED;
    }

    auto binding = service_->read_binding();
    info.vin = binding.vin;
    info.ecu_uid = binding.ecu_uid;
    return sec::ErrorCode::SUCCESS;
}

bool ProvToSecAdapter::is_connected() const {
    return service_ && service_->is_initialized();
}

std::string ProvToSecAdapter::get_service_status() const {
    if (!service_) {
        return "NOT_INITIALIZED";
    }
    return service_->is_initialized() ? "CONNECTED" : "NOT_INITIALIZED";
}

} // namespace diag
} // namespace tbox
