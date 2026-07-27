#include "diag_log_adapter.h"

namespace tbox::diag {

bool DiagLogAdapter::s_initialized = false;

tbox::fw::log::InitResult DiagLogAdapter::init(
    const std::string& service,
    const tbox::fw::log::LogConfig& config
) {
    if (s_initialized) {
        return tbox::fw::log::InitResult(
            tbox::fw::log::LogError::kInitFailed,
            "DiagLogAdapter already initialized"
        );
    }
    
    auto result = tbox::fw::log::Logger::init(service, config);
    if (result.error == tbox::fw::log::LogError::kOk) {
        s_initialized = true;
    }
    return result;
}

bool DiagLogAdapter::is_initialized() {
    return s_initialized;
}

tbox::fw::log::Logger DiagLogAdapter::transport() {
    return tbox::fw::log::Logger::get("transport");
}

tbox::fw::log::Logger DiagLogAdapter::session() {
    return tbox::fw::log::Logger::get("session");
}

tbox::fw::log::Logger DiagLogAdapter::uds_router() {
    return tbox::fw::log::Logger::get("uds_router");
}

tbox::fw::log::Logger DiagLogAdapter::downstream() {
    return tbox::fw::log::Logger::get("downstream");
}

tbox::fw::log::Logger DiagLogAdapter::response() {
    return tbox::fw::log::Logger::get("response");
}

tbox::fw::log::Logger DiagLogAdapter::ipc() {
    return tbox::fw::log::Logger::get("ipc");
}

} // namespace tbox::diag
