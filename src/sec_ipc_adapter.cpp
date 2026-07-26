#include "sec_ipc_adapter.h"
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include <iostream>

namespace tbox {
namespace diag {

SecIpcAdapter::SecIpcAdapter(std::shared_ptr<sec::SecClient> client)
    : client_(std::move(client)) {}

bool SecIpcAdapter::get_seed(uint8_t level, std::vector<uint8_t>& seed) {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC client not connected",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("get_seed"))}
        );
        return false;
    }

    auto result = client_->get_seed(level, seed);
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC get_seed failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("get_seed")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeInt(static_cast<int>(result)))}
        );
        return false;
    }

    {
        char level_buf[16];
        snprintf(level_buf, sizeof(level_buf), "0x%02X", level);
        DiagLogAdapter::downstream().info(
            "diag.downstream.sec_ipc_get_seed",
            "SEC IPC get_seed succeeded",
            {fw::log::Field(events::fields::SECURITY_LEVEL,
                fw::log::FieldValue::makeString(level_buf)),
             fw::log::Field("seed_size",
                fw::log::FieldValue::makeInt(static_cast<int64_t>(seed.size())))}
        );
    }
    return true;
}

bool SecIpcAdapter::verify_key(uint8_t level, const std::vector<uint8_t>& key) {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC client not connected",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("verify_key"))}
        );
        return false;
    }

    auto result = client_->verify_key(level, key);
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC verify_key failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("verify_key")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeInt(static_cast<int>(result)))}
        );
        return false;
    }

    {
        char level_buf[16];
        snprintf(level_buf, sizeof(level_buf), "0x%02X", level);
        DiagLogAdapter::downstream().info(
            "diag.downstream.sec_ipc_verify_key",
            "SEC IPC verify_key succeeded",
            {fw::log::Field(events::fields::SECURITY_LEVEL,
                fw::log::FieldValue::makeString(level_buf))}
        );
    }
    return true;
}

bool SecIpcAdapter::is_available() const {
    return client_ && client_->is_connected();
}

bool SecIpcAdapter::generate_key_pair() {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC client not connected",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("generate_key_pair"))}
        );
        return false;
    }

    auto result = client_->generate_key_pair();
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC generate_key_pair failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("generate_key_pair")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeInt(static_cast<int>(result)))}
        );
        return false;
    }

    DiagLogAdapter::downstream().info(
        "diag.downstream.sec_ipc_generate_key_pair",
        "SEC IPC generate_key_pair succeeded"
    );
    return true;
}

bool SecIpcAdapter::get_csr(std::vector<uint8_t>& csr_der) {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC client not connected",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("get_csr"))}
        );
        return false;
    }

    auto result = client_->get_csr(csr_der);
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC get_csr failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("get_csr")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeInt(static_cast<int>(result)))}
        );
        return false;
    }

    DiagLogAdapter::downstream().info(
        "diag.downstream.sec_ipc_get_csr",
        "SEC IPC get_csr succeeded",
        {fw::log::Field("csr_size",
            fw::log::FieldValue::makeInt(static_cast<int64_t>(csr_der.size())))}
    );
    return true;
}

bool SecIpcAdapter::submit_csr() {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC client not connected",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("submit_csr"))}
        );
        return false;
    }

    auto result = client_->submit_csr();
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC submit_csr failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("submit_csr")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeInt(static_cast<int>(result)))}
        );
        return false;
    }

    DiagLogAdapter::downstream().info(
        "diag.downstream.sec_ipc_submit_csr",
        "SEC IPC submit_csr succeeded"
    );
    return true;
}

bool SecIpcAdapter::inject_certificate(const std::vector<uint8_t>& cert_der) {
    if (!is_available()) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC client not connected",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("inject_certificate"))}
        );
        return false;
    }

    auto result = client_->inject_certificate(cert_der);
    if (result != sec::ErrorCode::SUCCESS) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC inject_certificate failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("inject_certificate")),
             fw::log::Field(events::fields::DOWNSTREAM_ERROR_CODE,
                fw::log::FieldValue::makeInt(static_cast<int>(result)))}
        );
        return false;
    }

    DiagLogAdapter::downstream().info(
        "diag.downstream.sec_ipc_inject_certificate",
        "SEC IPC inject_certificate succeeded"
    );
    return true;
}

} // namespace diag
} // namespace tbox