#pragma once

#include "prov_interface.h"
#include <string>
#include <map>

namespace tbox {
namespace diag {

class MockProvInterface : public ProvInterface {
public:
    MockProvInterface() = default;
    ~MockProvInterface() override = default;

    DiagErrorCode write_vin(const std::string& vin, const std::vector<uint8_t>& payload) override {
        if (!available_) {
            return DiagErrorCode::PROV_UNAVAILABLE;
        }
        if (fail_write_) {
            return DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED;
        }
        stored_vin_ = vin;
        stored_payload_ = payload;
        write_count_++;
        return DiagErrorCode::SUCCESS;
    }

    VinReadResult read_vin() override {
        VinReadResult result;
        if (!available_ || !read_valid_) {
            result.valid = false;
            return result;
        }
        result.vin = stored_vin_;
        result.bind_state = bind_state_;
        result.valid = true;
        return result;
    }

    bool is_available() const override {
        return available_;
    }

    void set_available(bool available) { available_ = available; }
    void set_fail_write(bool fail) { fail_write_ = fail; }
    void set_read_valid(bool valid) { read_valid_ = valid; }
    void set_stored_vin(const std::string& vin) { stored_vin_ = vin; }
    void set_bind_state(const std::string& state) { bind_state_ = state; }
    uint32_t get_write_count() const { return write_count_; }

private:
    bool available_ = true;
    bool fail_write_ = false;
    bool read_valid_ = true;
    std::string stored_vin_;
    std::vector<uint8_t> stored_payload_;
    std::string bind_state_ = "BOUND";
    uint32_t write_count_ = 0;
};

} // namespace diag
} // namespace tbox
