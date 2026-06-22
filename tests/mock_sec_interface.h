#pragma once

#include "sec_interface.h"
#include <map>
#include <vector>

namespace tbox {
namespace diag {

class MockSecInterface : public SecInterface {
public:
    MockSecInterface() = default;
    ~MockSecInterface() override = default;

    bool get_seed(uint8_t level, std::vector<uint8_t>& seed) override {
        if (!available_) {
            return false;
        }
        seed = default_seed_;
        return true;
    }

    bool verify_key(uint8_t level, const std::vector<uint8_t>& key) override {
        if (!available_) {
            return false;
        }
        auto it = expected_keys_.find(level);
        if (it != expected_keys_.end()) {
            return it->second == key;
        }
        // Default: accept any key
        return accept_all_keys_;
    }

    bool is_available() const override {
        return available_;
    }

    void set_available(bool available) { available_ = available; }
    void set_default_seed(const std::vector<uint8_t>& seed) { default_seed_ = seed; }
    void set_expected_key(uint8_t level, const std::vector<uint8_t>& key) {
        expected_keys_[level] = key;
    }
    void set_accept_all_keys(bool accept) { accept_all_keys_ = accept; }

private:
    bool available_ = true;
    bool accept_all_keys_ = true;
    std::vector<uint8_t> default_seed_ = {0x12, 0x34, 0x56, 0x78};
    std::map<uint8_t, std::vector<uint8_t>> expected_keys_;
};

} // namespace diag
} // namespace tbox
