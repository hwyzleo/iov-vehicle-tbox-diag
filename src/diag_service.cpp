#include "diag_service.h"
#include "uds_codec.h"
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
#include "ipc.h"
#include "diag_ipc_dispatcher.h"
#endif
#include <iostream>
#include <vector>

namespace tbox {
namespace diag {

DiagService::DiagService() {
    config_.config_file_path = "/etc/tbox/diag_config.yaml";
}

DiagService::DiagService(const DiagServiceConfig& config) : config_(config) {}

DiagService::~DiagService() {
    stop_ipc_server();
}

DiagErrorCode DiagService::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return DiagErrorCode::SUCCESS;
    }

    // Load configuration using framework-config
    auto result = load_config();
    if (result != DiagErrorCode::SUCCESS) {
        return result;
    }

    return initialize_submodules();
}

DiagErrorCode DiagService::initialize_submodules() {
    session_mgr_ = std::make_shared<SessionManager>();

    if (!sec_) {
        DiagLogAdapter::uds_router().error(
            events::DOWNSTREAM_CALL_FAILED,
            "SEC interface not set",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("sec")),
             fw::log::Field(events::fields::FAILURE_REASON,
                fw::log::FieldValue::makeString("interface_not_set"))}
        );
        return DiagErrorCode::SEC_UNAVAILABLE;
    }
    security_access_ = std::make_shared<SecurityAccess>(sec_);

    if (!prov_) {
        DiagLogAdapter::downstream().error(
            events::DOWNSTREAM_CALL_FAILED,
            "PROV interface not set",
            {fw::log::Field(events::fields::DOWNSTREAM,
                fw::log::FieldValue::makeString("prov")),
             fw::log::Field(events::fields::FAILURE_REASON,
                fw::log::FieldValue::makeString("interface_not_set"))}
        );
        return DiagErrorCode::PROV_UNAVAILABLE;
    }
    dispatcher_ = std::make_shared<ServiceDispatcher>(prov_, sec_, session_mgr_, security_access_);

    auto result = register_default_routes();
    if (result != DiagErrorCode::SUCCESS) {
        return result;
    }

    initialized_ = true;
    return DiagErrorCode::SUCCESS;
}

DiagErrorCode DiagService::register_default_routes() {
    dispatcher_->register_route(UdsService::ROUTINE_CONTROL, Rid::WRITE_VIN_ROUTINE,
                                "PROV", true);
    dispatcher_->register_route(UdsService::ROUTINE_CONTROL, Rid::GENERATE_KEY_PAIR,
                                "SEC", true);
    dispatcher_->register_route(UdsService::ROUTINE_CONTROL, Rid::READ_CSR,
                                "SEC", true);
    dispatcher_->register_route(UdsService::ROUTINE_CONTROL, Rid::INJECT_CERTIFICATE,
                                "SEC", true);
    dispatcher_->register_route(UdsService::READ_DATA_BY_IDENTIFIER, Did::VIN,
                                "PROV", false);
    dispatcher_->register_route(UdsService::READ_DATA_BY_IDENTIFIER, Did::BINDING_STATE,
                                "PROV", false);
    return DiagErrorCode::SUCCESS;
}

DiagResponse DiagService::process_request(const DiagRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        DiagResponse response;
        response.positive = false;
        response.service_id = 0x7F;
        response.nrc = Nrc::CONDITIONS_NOT_CORRECT;
        response.diag_error_code = error_code_to_string(DiagErrorCode::NOT_INITIALIZED);
        return response;
    }

    session_mgr_->check_s3_timeout();
    return dispatcher_->dispatch(request);
}

void DiagService::process_pending_requests() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return;
    }

    session_mgr_->check_s3_timeout();

    if (!transport_) {
        return;
    }

    if (!transport_->is_connected()) {
        transport_->connect();
        return;
    }

    auto raw = transport_->receive(0);
    if (raw.empty()) {
        return;
    }

    DiagRequest request;
    request.transport = transport_->get_transport_type();

    if (!UdsCodec::decode(raw, request)) {
        DiagLogAdapter::uds_router().error(
            events::UDS_DECODE_FAILED,
            "Failed to decode UDS request",
            {fw::log::Field(events::fields::PAYLOAD_SIZE,
                fw::log::FieldValue::makeInt(static_cast<int64_t>(raw.size())))}
        );
        return;
    }

    {
        char sid_buf[16], sub_buf[16], did_buf[16];
        snprintf(sid_buf, sizeof(sid_buf), "0x%02X", request.service_id);
        snprintf(sub_buf, sizeof(sub_buf), "0x%02X", request.sub_function);
        snprintf(did_buf, sizeof(did_buf), "0x%04X", request.did_or_rid);
        DiagLogAdapter::uds_router().info(
            events::UDS_REQUEST_COMPLETED,
            "UDS request received",
            {fw::log::Field(events::fields::SERVICE_ID,
                fw::log::FieldValue::makeString(sid_buf)),
             fw::log::Field(events::fields::SUB_FUNCTION,
                fw::log::FieldValue::makeString(sub_buf)),
             fw::log::Field(events::fields::DID_OR_RID,
                fw::log::FieldValue::makeString(did_buf)),
             fw::log::Field(events::fields::PAYLOAD_SIZE,
                fw::log::FieldValue::makeInt(static_cast<int64_t>(raw.size())))}
        );
    }

    DiagResponse response = dispatcher_->dispatch(request);

    auto resp_raw = UdsCodec::encode(response);
    if (!transport_->send(resp_raw)) {
        DiagLogAdapter::response().error(
            events::UDS_SEND_FAILED,
            "Failed to send UDS response"
        );
    }

    {
        char sid_buf[16], nrc_buf[16];
        snprintf(sid_buf, sizeof(sid_buf), "0x%02X", response.service_id);
        snprintf(nrc_buf, sizeof(nrc_buf), "0x%02X", response.nrc);
        DiagLogAdapter::response().info(
            "diag.uds.response.sent",
            "UDS response sent",
            {fw::log::Field(events::fields::POSITIVE,
                fw::log::FieldValue::makeBool(response.positive)),
             fw::log::Field(events::fields::SERVICE_ID,
                fw::log::FieldValue::makeString(sid_buf)),
             fw::log::Field(events::fields::NRC,
                fw::log::FieldValue::makeString(nrc_buf)),
             fw::log::Field(events::fields::PAYLOAD_SIZE,
                fw::log::FieldValue::makeInt(static_cast<int64_t>(resp_raw.size())))}
        );
    }
}

void DiagService::shutdown() {
    stop_ipc_server();
    std::lock_guard<std::mutex> lock(mutex_);
    if (transport_) {
        transport_->disconnect();
    }
    if (session_mgr_) {
        session_mgr_->release_session();
    }
    if (security_access_) {
        security_access_->reset();
    }
    initialized_ = false;
}

bool DiagService::is_initialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

DiagSession DiagService::get_current_session() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (session_mgr_) {
        return session_mgr_->get_session();
    }
    return DiagSession{};
}

void DiagService::set_transport(std::shared_ptr<TransportAdapter> transport) {
    std::lock_guard<std::mutex> lock(mutex_);
    transport_ = transport;
}

void DiagService::set_prov(std::shared_ptr<ProvInterface> prov) {
    std::lock_guard<std::mutex> lock(mutex_);
    prov_ = prov;
}

void DiagService::set_sec(std::shared_ptr<SecInterface> sec) {
    std::lock_guard<std::mutex> lock(mutex_);
    sec_ = sec;
    if (security_access_) {
        security_access_ = std::make_shared<SecurityAccess>(sec_);
    }
    if (dispatcher_) {
        dispatcher_ = std::make_shared<ServiceDispatcher>(prov_, sec_, session_mgr_, security_access_);
        register_default_routes();
    }
}

// ============================================================
// IPC server (framework-ipc)
// ============================================================

bool DiagService::start_ipc_server() {
#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
    if (!initialized_) {
        return false;
    }

    if (fw_ipc_server_) {
        return true;  // already started
    }

    // Create dispatcher
    ipc_dispatcher_ = std::make_unique<DiagIpcDispatcher>(this);

    // Create framework-ipc Server
    fw_ipc_server_ = std::make_unique<::tbox::fw::ipc::Server>(
        config_.ipc_socket_path, config_.ipc_config);

    // Register RequestHandler (adapt to dispatcher)
    auto* dispatcher_ptr = ipc_dispatcher_.get();
    auto request_handler = [dispatcher_ptr](uint32_t method_id,
                                            std::string_view params_json,
                                            int client_fd) -> std::string {
        return dispatcher_ptr->dispatch(method_id, params_json, client_fd);
    };

    // disconnect handler: only log, framework handles fd/subscription cleanup
    auto disconnect_handler = [](int client_fd) {
        DiagLogAdapter::ipc().debug(
            events::IPC_CLIENT_DISCONNECTED,
            "Client disconnected (framework)",
            {fw::log::Field("client_fd", fw::log::FieldValue::makeInt(client_fd))}
        );
    };

    if (!fw_ipc_server_->start(std::move(request_handler), std::move(disconnect_handler))) {
        fw_ipc_server_.reset();
        ipc_dispatcher_.reset();
        DiagLogAdapter::ipc().error(
            events::IPC_SERVER_START_FAILED,
            "Failed to start IPC server (framework-ipc)"
        );
        return false;
    }

    DiagLogAdapter::ipc().info(
        events::IPC_SERVER_STARTED,
        "IPC server started (framework-ipc)",
        {fw::log::Field("socket_path", fw::log::FieldValue::makeString(config_.ipc_socket_path))}
    );
    return true;
#else
    return false;
#endif
}

void DiagService::stop_ipc_server() {
#if defined(TBOX_DIAG_USE_FRAMEWORK_IPC)
    // Stop server first (waits for connection threads to exit), then reset members.
    // This order ensures no dispatch handler is running when we destroy the dispatcher.
    if (fw_ipc_server_) {
        fw_ipc_server_->stop();
        fw_ipc_server_.reset();
        ipc_dispatcher_.reset();
        DiagLogAdapter::ipc().info(
            events::IPC_SERVER_STOPPED,
            "IPC server stopped (framework-ipc)"
        );
    }
#endif
}

// ============================================================
// IPC query helpers (used by DiagIpcDispatcher)
// ============================================================

bool DiagService::is_tester_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transport_ && transport_->is_connected();
}

VinReadResult DiagService::get_vehicle_info() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!prov_) {
        return VinReadResult{};
    }
    return prov_->read_vin();
}

DiagErrorCode DiagService::load_config() {
    try {
        // Load configuration using framework-config
        auto& config_manager = hwyz::config::ConfigManager::instance();
        
        // Try to load configuration, but don't fail if config file doesn't exist
        // This allows tests to run without config files
        auto result = config_manager.load("diag");
        if (result != hwyz::config::ConfigError::kOk) {
            DiagLogAdapter::uds_router().warn(
                events::CONFIG_LOADED,
                "Failed to load configuration, using defaults",
                {fw::log::Field("error_code",
                    fw::log::FieldValue::makeInt(static_cast<uint32_t>(result)))}
            );
            
            // Set a null snapshot, apply_config will use defaults
            config_.config_snapshot = nullptr;
            return DiagErrorCode::SUCCESS;
        }

        // Get configuration snapshot
        config_.config_snapshot = config_manager.getSnapshot();
        if (!config_.config_snapshot) {
            DiagLogAdapter::uds_router().warn(
                events::CONFIG_LOADED,
                "Failed to get configuration snapshot, using defaults"
            );
            return DiagErrorCode::SUCCESS;
        }

        // Apply configuration
        return apply_config();
    } catch (const std::exception& e) {
        DiagLogAdapter::uds_router().error(
            events::CONFIG_LOADED,
            "Exception during configuration loading, using defaults",
            {fw::log::Field("exception",
                fw::log::FieldValue::makeString(e.what()))}
        );
        return DiagErrorCode::SUCCESS;
    }
}

DiagErrorCode DiagService::apply_config() {
    if (!config_.config_snapshot) {
        return DiagErrorCode::CONFIG_LOAD_FAILED;
    }

    try {
        // Apply timing configuration
        auto timing_section = config_.config_snapshot->getSection("timing");
        if (timing_section) {
            // Update timing constants (these will be used by session_manager and service_dispatcher)
            DiagLogAdapter::uds_router().info(
                events::CONFIG_LOADED,
                "Loaded timing configuration",
                {fw::log::Field("section",
                    fw::log::FieldValue::makeString("timing"))}
            );
        }

        // Apply security configuration
        auto security_section = config_.config_snapshot->getSection("security");
        if (security_section) {
            DiagLogAdapter::uds_router().info(
                events::CONFIG_LOADED,
                "Loaded security configuration",
                {fw::log::Field("section",
                    fw::log::FieldValue::makeString("security"))}
            );
        }

        // Apply transport configuration
        auto transport_section = config_.config_snapshot->getSection("transport");
        if (transport_section) {
            DiagLogAdapter::uds_router().info(
                events::CONFIG_LOADED,
                "Loaded transport configuration",
                {fw::log::Field("section",
                    fw::log::FieldValue::makeString("transport"))}
            );
        }

        return DiagErrorCode::SUCCESS;
    } catch (const std::exception& e) {
        DiagLogAdapter::uds_router().error(
            events::CONFIG_LOADED,
            "Exception during configuration application",
            {fw::log::Field("exception",
                fw::log::FieldValue::makeString(e.what()))}
        );
        return DiagErrorCode::CONFIG_LOAD_FAILED;
    }
}

} // namespace diag
} // namespace tbox
