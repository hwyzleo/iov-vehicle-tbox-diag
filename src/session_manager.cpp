#include "session_manager.h"
#include <iostream>

namespace tbox {
namespace diag {

SessionManager::SessionManager() {
    session_.session_type = SessionType::DEFAULT;
    session_.state = SessionState::IDLE;
    session_.security_unlocked = false;
    session_.last_tester_present_at = std::chrono::steady_clock::now();
}

DiagErrorCode SessionManager::switch_session(uint8_t session_type, uint16_t source_address,
                                              TransportType transport) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[SESSION] switch_session: requested=0x" << std::hex << (int)session_type
              << " current=0x" << (int)session_.session_type
              << " state=0x" << (int)session_.state << std::endl;

    // Check if session is already occupied by another tester
    if (session_.state == SessionState::ACTIVE &&
        session_.source_address != source_address &&
        session_.session_type != SessionType::DEFAULT) {
        return DiagErrorCode::SESSION_STATE_NOT_ALLOWED;
    }

    // Validate session type
    if (session_type != UdsSession::DEFAULT &&
        session_type != UdsSession::EXTENDED &&
        session_type != UdsSession::PROGRAMMING) {
        return DiagErrorCode::INVALID_REQUEST_FORMAT;
    }

    session_.source_address = source_address;
    session_.session_type = static_cast<SessionType>(session_type);
    session_.state = SessionState::ACTIVE;
    session_.transport = transport;
    session_.session_started_at = std::chrono::steady_clock::now();
    session_.last_tester_present_at = std::chrono::steady_clock::now();

    // Reset security when switching to default
    if (session_type == UdsSession::DEFAULT) {
        session_.security_unlocked = false;
    }

    return DiagErrorCode::SUCCESS;
}

DiagErrorCode SessionManager::handle_tester_present(uint16_t source_address) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (session_.state != SessionState::ACTIVE) {
        return DiagErrorCode::SESSION_STATE_NOT_ALLOWED;
    }

    // Only the owning tester can send TesterPresent
    if (session_.source_address != source_address) {
        return DiagErrorCode::SESSION_STATE_NOT_ALLOWED;
    }

    session_.last_tester_present_at = std::chrono::steady_clock::now();
    return DiagErrorCode::SUCCESS;
}

DiagSession SessionManager::get_session() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_;
}

void SessionManager::release_session() {
    std::lock_guard<std::mutex> lock(mutex_);
    session_.session_type = SessionType::DEFAULT;
    session_.state = SessionState::IDLE;
    session_.security_unlocked = false;
    session_.source_address = 0;
}

bool SessionManager::is_session_active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_.state == SessionState::ACTIVE;
}

bool SessionManager::is_in_non_default_session() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return session_.session_type != SessionType::DEFAULT;
}

bool SessionManager::check_s3_timeout() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (session_.session_type == SessionType::DEFAULT) {
        return false;  // Default session doesn't timeout
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - session_.last_tester_present_at).count();

    if (elapsed > Timing::S3_DEFAULT) {
        std::cout << "[SESSION] S3 timeout! elapsed=" << elapsed << "ms > " << Timing::S3_DEFAULT
                  << "ms, resetting to DEFAULT" << std::endl;
        session_.session_type = SessionType::DEFAULT;
        session_.state = SessionState::ACTIVE;
        session_.security_unlocked = false;
        return true;  // Timed out
    }

    return false;
}

void SessionManager::reset_to_default() {
    std::lock_guard<std::mutex> lock(mutex_);
    session_.session_type = SessionType::DEFAULT;
    session_.state = SessionState::ACTIVE;
    session_.security_unlocked = false;
    session_.last_tester_present_at = std::chrono::steady_clock::now();
}

} // namespace diag
} // namespace tbox
