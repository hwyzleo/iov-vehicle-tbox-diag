#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include "constants.h"

namespace tbox {
namespace diag {

enum class SessionType : uint8_t {
    DEFAULT = 0x01,
    PROGRAMMING = 0x02,
    EXTENDED = 0x03
};

enum class SessionState : uint8_t {
    IDLE,
    ACTIVE,
    SECURITY_UNLOCKED,
    TIMED_OUT
};

struct DiagSession {
    uint16_t source_address = 0;
    SessionType session_type = SessionType::DEFAULT;
    SessionState state = SessionState::IDLE;
    bool security_unlocked = false;
    TransportType transport = TransportType::DOIP;
    std::chrono::steady_clock::time_point last_tester_present_at;
    std::chrono::steady_clock::time_point session_started_at;
};

struct SecurityAccessState {
    uint8_t level = 0;
    bool seed_requested = false;
    bool unlocked = false;
    uint32_t attempt_count = 0;
    std::chrono::steady_clock::time_point locked_until;
    std::vector<uint8_t> requested_seed;
};

struct RoutingEntry {
    uint8_t service_id;
    uint16_t did_or_rid;
    std::string downstream;  // "PROV" or "SEC"
    bool requires_unlock;
};

struct DiagRequest {
    uint8_t service_id = 0;
    uint8_t sub_function = 0;
    uint16_t did_or_rid = 0;
    std::vector<uint8_t> payload;
    uint16_t source_address = 0;
    TransportType transport = TransportType::DOIP;
};

struct DiagResponse {
    bool positive = true;
    std::vector<uint8_t> payload;
    uint8_t nrc = 0;
    std::string diag_error_code;
    uint8_t service_id = 0;
    uint8_t sub_function = 0;
};

} // namespace diag
} // namespace tbox
