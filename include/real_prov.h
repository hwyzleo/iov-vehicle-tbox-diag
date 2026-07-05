#pragma once

#include "prov_interface.h"
#include "prov_client.h"
#include <memory>

namespace tbox {
namespace diag {

class RealProvAdapter : public ProvInterface {
public:
    explicit RealProvAdapter(std::shared_ptr<prov::ProvClient> client);
    ~RealProvAdapter() override = default;

    DiagErrorCode write_vin(const std::string& vin, const std::vector<uint8_t>& payload) override;
    VinReadResult read_vin() override;
    bool is_available() const override;
    bool reconnect() override;

private:
    std::shared_ptr<prov::ProvClient> client_;
};

} // namespace diag
} // namespace tbox
