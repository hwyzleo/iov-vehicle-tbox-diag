#include "real_prov.h"
#include "nrc_mapper.h"
#include <iostream>

namespace tbox {
namespace diag {

RealProvAdapter::RealProvAdapter(std::shared_ptr<prov::ProvService> service)
    : service_(std::move(service)) {}

DiagErrorCode RealProvAdapter::write_vin(const std::string& vin, const std::vector<uint8_t>& payload) {
    if (!is_available()) {
        std::cerr << "[REAL-PROV] Service not available" << std::endl;
        return DiagErrorCode::PROV_UNAVAILABLE;
    }

    std::cout << "[REAL-PROV] write_vin: " << vin << std::endl;

    auto result = service_->write_vin(vin);
    if (result != prov::ErrorCode::SUCCESS) {
        std::cerr << "[REAL-PROV] write_vin failed: "
                  << prov::error_code_to_string(result) << std::endl;
        return NrcMapper::prov_error_to_diag(static_cast<uint32_t>(result));
    }

    if (!payload.empty()) {
        std::cout << "[REAL-PROV] write_vehicle_config, size: " << payload.size() << std::endl;
        auto config_result = service_->write_vehicle_config(payload);
        if (config_result != prov::ErrorCode::SUCCESS) {
            std::cerr << "[REAL-PROV] write_vehicle_config failed: "
                      << prov::error_code_to_string(config_result) << std::endl;
            return NrcMapper::prov_error_to_diag(static_cast<uint32_t>(config_result));
        }
    }

    return DiagErrorCode::SUCCESS;
}

VinReadResult RealProvAdapter::read_vin() {
    VinReadResult result;

    if (!is_available()) {
        std::cerr << "[REAL-PROV] Service not available for read" << std::endl;
        result.valid = false;
        return result;
    }

    result.vin = service_->read_vin();
    auto state = service_->get_provision_state();

    switch (state) {
        case prov::ProvisionState::NONE:
            result.bind_state = "NONE";
            break;
        case prov::ProvisionState::VIN_WRITTEN:
            result.bind_state = "VIN_WRITTEN";
            break;
        case prov::ProvisionState::BOUND:
            result.bind_state = "BOUND";
            break;
        case prov::ProvisionState::FAILED:
            result.bind_state = "FAILED";
            break;
        default:
            result.bind_state = "UNKNOWN";
            break;
    }

    result.valid = !result.vin.empty();
    std::cout << "[REAL-PROV] read_vin: " << result.vin
              << ", state: " << result.bind_state << std::endl;

    return result;
}

bool RealProvAdapter::is_available() const {
    return service_ && service_->is_initialized();
}

} // namespace diag
} // namespace tbox
