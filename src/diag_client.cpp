#include "tbox/diag/client.h"
#include "diag_retry_policy.h"
#include "diag_ipc_protocol.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstring>

#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
#include "ipc.h"
#include <memory>
#endif

namespace tbox {
namespace diag {

// ============================================================
// DiagClientError helpers
// ============================================================

std::string error_code_to_string(DiagClientError code) {
    switch (code) {
        case DiagClientError::SUCCESS:            return "SUCCESS";
        case DiagClientError::CONNECTION_FAILED:  return "CONNECTION_FAILED";
        case DiagClientError::TIMEOUT:            return "TIMEOUT";
        case DiagClientError::SERVICE_ERROR:      return "SERVICE_ERROR";
        case DiagClientError::INTERNAL_ERROR:     return "INTERNAL_ERROR";
        case DiagClientError::NOT_CONNECTED:      return "NOT_CONNECTED";
        default:                                  return "UNKNOWN";
    }
}

// ============================================================
// DiagClient::Impl
// ============================================================
class DiagClient::Impl {
public:
    explicit Impl(const std::string& socket_path)
#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
        : socket_path_(socket_path),
          fw_client_(std::make_unique<::tbox::fw::ipc::Client>(socket_path_))
#else
        : socket_path_(socket_path)
#endif
    {
    }

    ~Impl() {
        disconnect();
    }

    bool connect() {
#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
        return fw_client_->connect();
#else
        return false;
#endif
    }

    void disconnect() {
#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
        fw_client_->disconnect();
#endif
    }

    bool is_connected() const {
#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
        return fw_client_->is_connected();
#else
        return false;
#endif
    }

    /// Send request and receive response
    /// @return (ok, biz_status_code, response_json)
    std::tuple<bool, int32_t, std::string>
    send_request(uint32_t method_id, const std::string& params_json) {
#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
        // DiagRetryPolicy: decide whether to allow auto-retry
        bool retry_allowed = DiagRetryPolicy::should_retry(method_id);

        std::pair<int32_t, std::string> result;
        if (retry_allowed) {
            result = fw_client_->call(method_id, params_json);
        } else {
            // callOnce: no auto-retry, transport failure = unknown outcome
            result = fw_client_->callOnce(method_id, params_json);
        }

        auto [fw_status, response_json] = result;

        if (fw_status < 0) {
            // Client transport error
            return {false, 0, ""};
        }

        if (fw_status > 0) {
            // Server transport/handler error (FW-03xx)
            return {false, 0, ""};
        }

        // fw_status == 0: transport success, extract business status from JSON
        int32_t biz_status = 0;
        try {
            auto j = nlohmann::json::parse(response_json);
            biz_status = j.value("status", 0);
        } catch (...) {
            // JSON parse failure, keep biz_status = 0
        }
        return {true, biz_status, response_json};
#else
        return {false, 0, ""};
#endif
    }

private:
    std::string socket_path_;
#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
    std::unique_ptr<::tbox::fw::ipc::Client> fw_client_;
#endif
};

// ============================================================
// DiagClient facade
// ============================================================

DiagClient::DiagClient(const std::string& socket_path)
    : impl_(std::make_unique<Impl>(socket_path)) {
}

DiagClient::~DiagClient() = default;

bool DiagClient::connect() {
    return impl_->connect();
}

void DiagClient::disconnect() {
    impl_->disconnect();
}

bool DiagClient::is_connected() const {
    return impl_->is_connected();
}

DiagClientError DiagClient::get_service_status(DiagServiceStatus& status) {
    if (!is_connected()) {
        return DiagClientError::NOT_CONNECTED;
    }

    auto [ok, biz_status, response_json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::GET_SERVICE_STATUS), "{}");
    if (!ok) return DiagClientError::CONNECTION_FAILED;
    if (biz_status != 0) return DiagClientError::SERVICE_ERROR;

    try {
        auto j = nlohmann::json::parse(response_json);
        status.initialized = j.value("initialized", false);
        status.session_type = j.value("session_type", "UNKNOWN");
        status.session_state = j.value("session_state", "UNKNOWN");
        status.security_unlocked = j.value("security_unlocked", false);
        status.transport = j.value("transport", "UNKNOWN");
    } catch (const std::exception&) {
        return DiagClientError::INTERNAL_ERROR;
    }
    return DiagClientError::SUCCESS;
}

DiagClientError DiagClient::get_vehicle_info(VehicleInfo& info) {
    if (!is_connected()) {
        return DiagClientError::NOT_CONNECTED;
    }

    auto [ok, biz_status, response_json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::GET_VEHICLE_INFO), "{}");
    if (!ok) return DiagClientError::CONNECTION_FAILED;
    if (biz_status != 0) return DiagClientError::SERVICE_ERROR;

    try {
        auto j = nlohmann::json::parse(response_json);
        info.valid = j.value("valid", false);
        if (info.valid) {
            info.vin = j.value("vin", "");
            info.bind_state = j.value("bind_state", "UNKNOWN");
        }
    } catch (const std::exception&) {
        return DiagClientError::INTERNAL_ERROR;
    }
    return DiagClientError::SUCCESS;
}

DiagClientError DiagClient::is_tester_connected(bool& connected) {
    if (!is_connected()) {
        return DiagClientError::NOT_CONNECTED;
    }

    auto [ok, biz_status, response_json] = impl_->send_request(
        static_cast<uint32_t>(ipc::MethodId::IS_TESTER_CONNECTED), "{}");
    if (!ok) return DiagClientError::CONNECTION_FAILED;
    if (biz_status != 0) return DiagClientError::SERVICE_ERROR;

    try {
        auto j = nlohmann::json::parse(response_json);
        connected = j.value("connected", false);
    } catch (const std::exception&) {
        return DiagClientError::INTERNAL_ERROR;
    }
    return DiagClientError::SUCCESS;
}

} // namespace diag
} // namespace tbox
