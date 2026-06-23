#pragma once

#include "sec_interface.h"
#include "error_codes.h"
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

    bool generate_key_pair() override {
        return generate_key_pair_result_;
    }

    bool get_csr(std::vector<uint8_t>& csr_der) override {
        csr_der = csr_data_;
        return get_csr_result_;
    }

    bool submit_csr() override {
        return submit_csr_result_;
    }

    bool inject_certificate(const std::vector<uint8_t>& cert_der) override {
        injected_certificate_ = cert_der;
        return inject_certificate_result_;
    }

    void set_generate_key_pair_result(bool result) { generate_key_pair_result_ = result; }
    void set_csr_data(const std::vector<uint8_t>& data) { csr_data_ = data; }
    void set_get_csr_result(bool result) { get_csr_result_ = result; }
    void set_submit_csr_result(bool result) { submit_csr_result_ = result; }
    void set_inject_certificate_result(bool result) { inject_certificate_result_ = result; }
    const std::vector<uint8_t>& get_injected_certificate() const { return injected_certificate_; }

private:
    bool available_ = true;
    bool accept_all_keys_ = true;
    std::vector<uint8_t> default_seed_ = {0x12, 0x34, 0x56, 0x78};
    std::map<uint8_t, std::vector<uint8_t>> expected_keys_;
    bool generate_key_pair_result_ = true;
    std::vector<uint8_t> csr_data_ = {0x30, 0x82, 0x01, 0x00};
    bool get_csr_result_ = true;
    bool submit_csr_result_ = true;
    bool inject_certificate_result_ = true;
    std::vector<uint8_t> injected_certificate_;
};

} // namespace diag
} // namespace tbox
