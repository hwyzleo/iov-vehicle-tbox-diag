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
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC service not available",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("get_seed"))}
        );
        return false;
    }

    {
        char level_buf[16];
        snprintf(level_buf, sizeof(level_buf), "0x%02X", level);
        DiagLogAdapter::downstream().info(
            "diag.downstream.get_seed",
            "SEC get_seed",
            {fw::log::Field(events::fields::SECURITY_LEVEL,
                fw::log::FieldValue::makeString(level_buf))}
        );
    }
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
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC service not available",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("verify_key"))}
        );
        return false;
    }

    {
        char level_buf[16];
        snprintf(level_buf, sizeof(level_buf), "0x%02X", level);
        DiagLogAdapter::downstream().info(
            "diag.downstream.verify_key",
            "SEC verify_key",
            {fw::log::Field(events::fields::SECURITY_LEVEL,
                fw::log::FieldValue::makeString(level_buf))}
        );
    }
    auto result = service_->verify_key(level, key);
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC verify_key failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("verify_key")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeString(
                    sec::error_code_to_string(result)))}
        );
        return false;
    }

    return true;
}

bool RealSecAdapter::is_available() const {
    return service_ && service_->is_initialized();
}

bool RealSecAdapter::generate_key_pair() {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC service not available",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("generate_key_pair"))}
        );
        return false;
    }

    DiagLogAdapter::downstream().info(
        "diag.downstream.generate_key_pair",
        "SEC generate_key_pair"
    );
    auto result = service_->generate_key_pair();
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC generate_key_pair failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("generate_key_pair")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeString(
                    sec::error_code_to_string(result)))}
        );
        return false;
    }

    return true;
}

bool RealSecAdapter::get_csr(std::vector<uint8_t>& csr_der) {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC service not available",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("get_csr"))}
        );
        return false;
    }

    DiagLogAdapter::downstream().info(
        "diag.downstream.get_csr",
        "SEC get_csr"
    );
    auto result = service_->get_csr(csr_der);
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC get_csr failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("get_csr")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeString(
                    sec::error_code_to_string(result)))}
        );
        return false;
    }

    return true;
}

bool RealSecAdapter::submit_csr() {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC service not available",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("submit_csr"))}
        );
        return false;
    }

    DiagLogAdapter::downstream().info(
        "diag.downstream.submit_csr",
        "SEC submit_csr"
    );
    auto result = service_->submit_csr();
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC submit_csr failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("submit_csr")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeString(
                    sec::error_code_to_string(result)))}
        );
        return false;
    }

    return true;
}

bool RealSecAdapter::inject_certificate(const std::vector<uint8_t>& cert_der) {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC service not available",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("inject_certificate"))}
        );
        return false;
    }

    DiagLogAdapter::downstream().info(
        "diag.downstream.inject_certificate",
        "SEC inject_certificate"
    );
    auto result = service_->inject_certificate(cert_der);
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC inject_certificate failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("inject_certificate")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeString(
                    sec::error_code_to_string(result)))}
        );
        return false;
    }

    return true;
}

} // namespace diag
} // namespace tbox
