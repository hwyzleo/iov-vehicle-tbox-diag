#pragma once

#include <cstdint>
#include <string>

namespace tbox {
namespace diag {

enum class DiagErrorCode : uint32_t {
    SUCCESS = 0,

    // DIAG-1001: Transport connection / session establishment failed
    TRANSPORT_CONNECTION_FAILED = 1001,

    // DIAG-1002: Session state does not allow this request
    SESSION_STATE_NOT_ALLOWED = 1002,

    // DIAG-1003: Security access not unlocked / authentication failed
    SECURITY_ACCESS_DENIED = 1003,

    // DIAG-1004: Request format / parameter invalid
    INVALID_REQUEST_FORMAT = 1004,

    // DIAG-1005: Downstream service (PROV) execution failed
    DOWNSTREAM_EXECUTION_FAILED = 1005,

    // DIAG-1006: Request processing timeout
    REQUEST_TIMEOUT = 1006,

    // Internal error codes
    INTERNAL_ERROR = 9999,
    NOT_INITIALIZED = 9998,
    ROUTING_NOT_FOUND = 9997,
    SEC_UNAVAILABLE = 9996,
    PROV_UNAVAILABLE = 9995
};

std::string error_code_to_string(DiagErrorCode code);
std::string error_code_to_description(DiagErrorCode code);
DiagErrorCode nrc_to_diag_error(uint8_t nrc);

} // namespace diag
} // namespace tbox
