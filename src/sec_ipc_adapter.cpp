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

    // 空闲超时被服务端关闭的连接，在首次调用时才会被发现：此时 is_connected()
    // 仍为 true，callOnce 发送失败并返回 CONNECTION_FAILED。重新建链后重试一次。
    // 仅对 get_seed 这样做是安全的：传输失败说明服务端没收到请求，而重新取种子
    // 本身无副作用（DIAG 只保留最后一次下发的 seed）。verify_key 不做重试，
    // 以免重放导致失败计数重复累加。
    if (result == sec::ErrorCode::CONNECTION_FAILED && ensure_connected()) {
        DiagLogAdapter::downstream().warn(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC IPC get_seed hit a stale connection, retrying once after reconnect",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc")),
             fw::log::Field(events::fields::OPERATION,
                fw::log::FieldValue::makeString("get_seed"))}
        );
        result = client_->get_seed(level, seed);
    }

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

bool SecIpcAdapter::ensure_connected() const {
    if (!client_) {
        return false;
    }
    if (client_->is_connected()) {
        return true;
    }

    std::lock_guard<std::mutex> lock(connect_mutex_);
    // 双检：可能已被并发调用重连成功。
    if (client_->is_connected()) {
        return true;
    }

    auto now = std::chrono::steady_clock::now();
    if (connect_attempted_ && now - last_connect_attempt_ < kReconnectMinInterval) {
        return false;
    }
    connect_attempted_ = true;
    last_connect_attempt_ = now;

    // framework-ipc 的 callOnce 在传输失败后只把连接标记为断开，不会自动重连；
    // SEC 服务端又会主动关闭空闲连接。这里显式重连，否则 one-shot 方法
    // （GET_SEED / VERIFY_KEY）一旦遇到一次断连就永久不可用。
    const bool ok = client_->connect();
    if (ok) {
        DiagLogAdapter::downstream().info(
            events::SEC_CONNECTED,
            "SEC IPC reconnected",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc"))}
        );
    } else {
        DiagLogAdapter::downstream().warn(
            events::SEC_CONNECTION_FAILED,
            "SEC IPC reconnect failed",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec-ipc"))}
        );
    }
    return ok;
}

bool SecIpcAdapter::is_available() const {
    return ensure_connected();
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