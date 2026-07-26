#include "real_sec_adapter.h"
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include <iostream>

namespace tbox {
namespace diag {

RealSecAdapter::RealSecAdapter(std::shared_ptr<sec::SecService> service)
    : service_(std::move(service)) {}

bool RealSecAdapter::get_seed(uint8_t level, std::vector<uint8_t>& seed) {
    if (!is_available()) {
        std::cerr << "[REAL-SEC] Service not available" << std::endl;
        return false;
    }

    std::cout << "[REAL-SEC] get_seed level=" << static_cast<int>(level) << std::endl;
    auto result = service_->get_seed(level, seed);
    if (result != sec::ErrorCode::SUCCESS) {
        // 记录下游调用失败
        tbox::diag::DiagLogAdapter::downstream().error(
            tbox::diag::events::DOWNSTREAM_CALL_FAILED,
            "SEC call failed",
            {
                tbox::fw::log::Field(tbox::diag::events::fields::DOWNSTREAM, 
                    tbox::fw::log::FieldValue::makeString("sec")),
                tbox::fw::log::Field(tbox::diag::events::fields::OPERATION, 
                    tbox::fw::log::FieldValue::makeString("get_seed")),
                tbox::fw::log::Field(tbox::diag::events::fields::DOWNSTREAM_ERROR_CODE, 
                    tbox::fw::log::FieldValue::makeInt(static_cast<int>(result)))
            }
        );
        return false;
    }

    return true;
}

bool RealSecAdapter::verify_key(uint8_t level, const std::vector<uint8_t>& key) {
    if (!is_available()) {
        std::cerr << "[REAL-SEC] Service not available" << std::endl;
        return false;
    }

    std::cout << "[REAL-SEC] verify_key level=" << static_cast<int>(level) << std::endl;
    auto result = service_->verify_key(level, key);
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[REAL-SEC] verify_key failed: "
                  << sec::error_code_to_string(result) << std::endl;
        return false;
    }

    return true;
}

bool RealSecAdapter::is_available() const {
    return service_ && service_->is_initialized();
}

bool RealSecAdapter::generate_key_pair() {
    if (!is_available()) {
        std::cerr << "[REAL-SEC] Service not available" << std::endl;
        return false;
    }

    std::cout << "[REAL-SEC] generate_key_pair" << std::endl;
    auto result = service_->generate_key_pair();
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[REAL-SEC] generate_key_pair failed: "
                  << sec::error_code_to_string(result) << std::endl;
        return false;
    }

    return true;
}

bool RealSecAdapter::get_csr(std::vector<uint8_t>& csr_der) {
    if (!is_available()) {
        std::cerr << "[REAL-SEC] Service not available" << std::endl;
        return false;
    }

    std::cout << "[REAL-SEC] get_csr" << std::endl;
    auto result = service_->get_csr(csr_der);
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[REAL-SEC] get_csr failed: "
                  << sec::error_code_to_string(result) << std::endl;
        return false;
    }

    return true;
}

bool RealSecAdapter::submit_csr() {
    if (!is_available()) {
        std::cerr << "[REAL-SEC] Service not available" << std::endl;
        return false;
    }

    std::cout << "[REAL-SEC] submit_csr" << std::endl;
    auto result = service_->submit_csr();
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[REAL-SEC] submit_csr failed: "
                  << sec::error_code_to_string(result) << std::endl;
        return false;
    }

    return true;
}

bool RealSecAdapter::inject_certificate(const std::vector<uint8_t>& cert_der) {
    if (!is_available()) {
        std::cerr << "[REAL-SEC] Service not available" << std::endl;
        return false;
    }

    std::cout << "[REAL-SEC] inject_certificate" << std::endl;
    auto result = service_->inject_certificate(cert_der);
    if (result != sec::ErrorCode::SUCCESS) {
        std::cerr << "[REAL-SEC] inject_certificate failed: "
                  << sec::error_code_to_string(result) << std::endl;
        return false;
    }

    return true;
}

} // namespace diag
} // namespace tbox
