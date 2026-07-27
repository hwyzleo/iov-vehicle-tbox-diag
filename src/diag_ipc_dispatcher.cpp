#include "diag_ipc_dispatcher.h"
#include "diag_service.h"
#include "diag_ipc_protocol.h"
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include "data_models.h"
#include "error_codes.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace tbox {
namespace diag {

namespace {

// FW-0305: JSON/base64 serialization failed
constexpr int32_t FW_SERIALIZATION_FAILED = 305;
// FW-0306: unknown method or request handler failed
constexpr int32_t FW_HANDLER_FAILED = 306;

std::string make_json_response(int32_t status, const nlohmann::json& payload) {
    nlohmann::json j = payload;
    j["status"] = status;
    return j.dump();
}

} // anonymous namespace

DiagIpcDispatcher::DiagIpcDispatcher(DiagService* service)
    : service_(service) {
}

std::string DiagIpcDispatcher::dispatch(uint32_t method_id,
                                         std::string_view params_json,
                                         int client_fd) {
    (void)client_fd;  // 当前无订阅需求，预留
    (void)params_json;  // 当前方法均无参数

    auto start = std::chrono::steady_clock::now();

    // IPC 日志只记录 method_id 和 payload_bytes，不记录原始 JSON
    DiagLogAdapter::ipc().debug(
        events::IPC_DISPATCH,
        "Dispatching request",
        {fw::log::Field(events::fields::METHOD_ID,
            fw::log::FieldValue::makeInt(static_cast<int>(method_id))),
         fw::log::Field(events::fields::PAYLOAD_BYTES,
            fw::log::FieldValue::makeInt(static_cast<int>(params_json.size())))}
    );

    std::pair<int32_t, std::string> result{FW_HANDLER_FAILED, "{}"};

    try {
        switch (static_cast<ipc::MethodId>(method_id)) {
            case ipc::MethodId::GET_SERVICE_STATUS:
                result = handle_get_service_status();
                break;
            case ipc::MethodId::GET_VEHICLE_INFO:
                result = handle_get_vehicle_info();
                break;
            case ipc::MethodId::IS_TESTER_CONNECTED:
                result = handle_is_tester_connected();
                break;
            default:
                DiagLogAdapter::ipc().warn(
                    events::IPC_UNKNOWN_METHOD,
                    "Unknown method ID",
                    {fw::log::Field(events::fields::METHOD_ID,
                        fw::log::FieldValue::makeInt(static_cast<int>(method_id)))}
                );
                result = {FW_HANDLER_FAILED, nlohmann::json({{"error", "Unknown method"}}).dump()};
                break;
        }
    } catch (const nlohmann::json::exception& e) {
        DiagLogAdapter::ipc().error(
            "diag.ipc.json_error",
            "JSON serialization error in dispatch",
            {fw::log::Field(events::fields::METHOD_ID,
                fw::log::FieldValue::makeInt(static_cast<int>(method_id))),
             fw::log::Field("what", fw::log::FieldValue::makeString(e.what()))}
        );
        result = {FW_SERIALIZATION_FAILED, nlohmann::json({{"error", "JSON error"}}).dump()};
    } catch (const std::exception& e) {
        DiagLogAdapter::ipc().error(
            "diag.ipc.dispatch_exception",
            "Exception in dispatch",
            {fw::log::Field(events::fields::METHOD_ID,
                fw::log::FieldValue::makeInt(static_cast<int>(method_id))),
             fw::log::Field("what", fw::log::FieldValue::makeString(e.what()))}
        );
        result = {FW_HANDLER_FAILED, nlohmann::json({{"error", "Internal error"}}).dump()};
    } catch (...) {
        DiagLogAdapter::ipc().error(
            "diag.ipc.dispatch_exception",
            "Unknown exception in dispatch",
            {fw::log::Field(events::fields::METHOD_ID,
                fw::log::FieldValue::makeInt(static_cast<int>(method_id)))}
        );
        result = {FW_HANDLER_FAILED, nlohmann::json({{"error", "Unknown exception"}}).dump()};
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // IPC 日志只记录 method_id、status、duration，不记录响应内容
    DiagLogAdapter::ipc().debug(
        events::IPC_DISPATCHED,
        "Request dispatched",
        {fw::log::Field(events::fields::METHOD_ID,
            fw::log::FieldValue::makeInt(static_cast<int>(method_id))),
         fw::log::Field("status",
            fw::log::FieldValue::makeInt(result.first)),
         fw::log::Field(events::fields::DURATION_MS,
            fw::log::FieldValue::makeInt(duration_ms))}
    );

    // 构建最终 JSON：payload + status 字段
    try {
        auto payload = nlohmann::json::parse(result.second);
        return make_json_response(result.first, payload);
    } catch (...) {
        return make_json_response(result.first, nlohmann::json({{"error", "Invalid response"}}));
    }
}

// ============================================================
// Method handlers
// ============================================================

std::pair<int32_t, std::string> DiagIpcDispatcher::handle_get_service_status() {
    bool initialized = service_->is_initialized();
    DiagSession session = service_->get_current_session();

    nlohmann::json j;
    j["initialized"] = initialized;
    j["session_type"] = session_to_string(session.session_type);

    // Convert SessionState to string
    switch (session.state) {
        case SessionState::IDLE:             j["session_state"] = "IDLE"; break;
        case SessionState::ACTIVE:           j["session_state"] = "ACTIVE"; break;
        case SessionState::SECURITY_UNLOCKED: j["session_state"] = "SECURITY_UNLOCKED"; break;
        case SessionState::TIMED_OUT:        j["session_state"] = "TIMED_OUT"; break;
        default:                             j["session_state"] = "UNKNOWN"; break;
    }

    j["security_unlocked"] = session.security_unlocked;
    j["transport"] = (session.transport == TransportType::DOIP) ? "DOIP" : "DOCAN";

    return {static_cast<int32_t>(DiagErrorCode::SUCCESS), j.dump()};
}

std::pair<int32_t, std::string> DiagIpcDispatcher::handle_get_vehicle_info() {
    VinReadResult info = service_->get_vehicle_info();

    nlohmann::json j;
    j["valid"] = info.valid;
    if (info.valid) {
        j["vin"] = info.vin;
        j["bind_state"] = info.bind_state;
    }
    return {static_cast<int32_t>(DiagErrorCode::SUCCESS), j.dump()};
}

std::pair<int32_t, std::string> DiagIpcDispatcher::handle_is_tester_connected() {
    bool connected = service_->is_tester_connected();

    nlohmann::json j;
    j["connected"] = connected;
    return {static_cast<int32_t>(DiagErrorCode::SUCCESS), j.dump()};
}

} // namespace diag
} // namespace tbox
