#pragma once

#include "prov_interface.h"
#include <string>
#include <vector>
#include <iostream>

namespace tbox {
namespace diag {

class StubProvInterface : public ProvInterface {
public:
    StubProvInterface() = default;
    ~StubProvInterface() override = default;

    DiagErrorCode write_vin(const std::string& vin, const std::vector<uint8_t>& payload) override {
        std::cout << "[STUB-PROV] write_vin: " << vin << std::endl;
        stored_vin_ = vin;
        return DiagErrorCode::SUCCESS;
    }

    VinReadResult read_vin() override {
        std::cout << "[STUB-PROV] read_vin: " << stored_vin_ << std::endl;
        VinReadResult result;
        result.vin = stored_vin_;
        result.bind_state = stored_vin_.empty() ? "NONE" : "BOUND";
        result.valid = true;
        return result;
    }

    bool is_available() const override {
        return true;
    }

private:
    std::string stored_vin_;
};

} // namespace diag
} // namespace tbox
