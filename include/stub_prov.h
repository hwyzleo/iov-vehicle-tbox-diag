#pragma once

#include "prov_interface.h"
#include "diag_log_adapter.h"
#include <string>
#include <vector>

namespace tbox {
namespace diag {

class StubProvInterface : public ProvInterface {
public:
    StubProvInterface() = default;
    ~StubProvInterface() override = default;

    DiagErrorCode write_vin(const std::string& vin, const std::vector<uint8_t>& payload) override {
        DiagLogAdapter::downstream().info(
            "diag.downstream.stub_write_vin",
            "STUB-PROV write_vin",
            {fw::log::Field("vin",
                fw::log::FieldValue::makeString(vin))}
        );
        stored_vin_ = vin;
        return DiagErrorCode::SUCCESS;
    }

    VinReadResult read_vin() override {
        DiagLogAdapter::downstream().info(
            "diag.downstream.stub_read_vin",
            "STUB-PROV read_vin"
        );
        VinReadResult result;
        result.vin = stored_vin_;
        result.bind_state = stored_vin_.empty() ? "NONE" : "BOUND";
        result.valid = true;
        return result;
    }

    bool is_available() const override {
        return true;
    }

    bool reconnect() override {
        return true;
    }

private:
    std::string stored_vin_ = "1HGBH41JXMN109186";
};

} // namespace diag
} // namespace tbox
