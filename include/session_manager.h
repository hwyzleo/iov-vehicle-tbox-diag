#pragma once

#include "data_models.h"
#include "constants.h"
#include "error_codes.h"
#include <memory>
#include <mutex>
#include <chrono>

namespace tbox {
namespace diag {

class SessionManager {
public:
    SessionManager();
    virtual ~SessionManager() = default;

    virtual DiagErrorCode switch_session(uint8_t session_type, uint16_t source_address, TransportType transport);
    virtual DiagErrorCode handle_tester_present(uint16_t source_address);
    virtual DiagSession get_session() const;
    virtual void release_session();
    virtual bool is_session_active() const;
    virtual bool is_in_non_default_session() const;
    virtual bool check_s3_timeout();
    virtual void reset_to_default();

protected:
    DiagSession session_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace diag
} // namespace tbox
