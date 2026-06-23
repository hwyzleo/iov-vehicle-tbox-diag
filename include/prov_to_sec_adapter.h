#pragma once

#include "prov_service_interface.h"
#include "prov_service.h"
#include <memory>

namespace tbox {
namespace diag {

class ProvToSecAdapter : public sec::ProvServiceInterface {
public:
    explicit ProvToSecAdapter(std::shared_ptr<prov::ProvService> service);
    ~ProvToSecAdapter() override = default;

    sec::ErrorCode initialize() override;
    sec::ErrorCode get_vehicle_info(sec::VehicleInfo& info) override;
    bool is_connected() const override;
    std::string get_service_status() const override;

private:
    std::shared_ptr<prov::ProvService> service_;
};

} // namespace diag
} // namespace tbox
