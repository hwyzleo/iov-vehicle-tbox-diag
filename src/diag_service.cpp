#include "diag_service.h"
#include "uds_codec.h"
#include <iostream>
#include <vector>

namespace tbox {
namespace diag {

DiagService::DiagService() {
    config_.config_file_path = "/etc/tbox/diag_config.yaml";
}

DiagService::DiagService(const DiagServiceConfig& config) : config_(config) {}

DiagErrorCode DiagService::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return DiagErrorCode::SUCCESS;
    }

    return initialize_submodules();
}

DiagErrorCode DiagService::initialize_submodules() {
    session_mgr_ = std::make_shared<SessionManager>();

    if (!sec_) {
        std::cerr << "DIAG: SEC interface not set" << std::endl;
        return DiagErrorCode::SEC_UNAVAILABLE;
    }
    security_access_ = std::make_shared<SecurityAccess>(sec_);

    if (!prov_) {
        std::cerr << "DIAG: PROV interface not set" << std::endl;
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
        std::cerr << "[DIAG] Failed to decode UDS request" << std::endl;
        return;
    }

    std::cout << "[DIAG] RX SID=0x" << std::hex << static_cast<int>(request.service_id)
              << std::dec << std::endl;

    DiagResponse response = dispatcher_->dispatch(request);

    auto resp_raw = UdsCodec::encode(response);
    if (!transport_->send(resp_raw)) {
        std::cerr << "[DIAG] Failed to send response" << std::endl;
    }

    std::cout << "[DIAG] TX " << (response.positive ? "POS" : "NEG")
              << " SID=0x" << std::hex << static_cast<int>(response.service_id)
              << std::dec << std::endl;
}

void DiagService::shutdown() {
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

} // namespace diag
} // namespace tbox
