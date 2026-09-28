#pragma once

#include <cstdint>
#include <string>
#include "config.h"

namespace tbox {
namespace diag {

// UDS Service IDs (ISO 14229)
namespace UdsService {
    constexpr uint8_t DIAGNOSTIC_SESSION_CONTROL = 0x10;
    constexpr uint8_t TESTER_PRESENT = 0x3E;
    constexpr uint8_t SECURITY_ACCESS = 0x27;
    constexpr uint8_t READ_DATA_BY_IDENTIFIER = 0x22;
    constexpr uint8_t ROUTINE_CONTROL = 0x31;
}

// UDS Security Levels (SecurityAccess 0x27 sub-function based)
// NOTE: 0x27 is the SecurityAccess *service* ID, not a security level.
// Levels are addressed via sub-functions: level-1 requestSeed=0x01,
// sendKey=0x02. Sub-function bit7 is suppressPosRspMsgIndicationBit and
// must be masked (sub_function & 0x7F) before level identification.
// (TBOX-DIAG-DSN-CR-005)
namespace UdsSecurityLevel {
    constexpr uint8_t LEVEL_0 = 0x00;
    constexpr uint8_t LEVEL_1 = 0x01;  // level-1 requestSeed sub-function / unlock state key
}

// UDS Session Types
namespace UdsSession {
    constexpr uint8_t DEFAULT = 0x01;
    constexpr uint8_t EXTENDED = 0x03;
    constexpr uint8_t PROGRAMMING = 0x02;
}

// DID definitions
namespace Did {
    constexpr uint16_t VIN = 0xF190;
    constexpr uint16_t BINDING_STATE = 0xF191;
}

// RID definitions
namespace Rid {
    constexpr uint16_t WRITE_VIN_ROUTINE = 0xFF00;
    constexpr uint16_t GENERATE_KEY_PAIR = 0xFF01;
    constexpr uint16_t READ_CSR = 0xFF02;
    constexpr uint16_t INJECT_CERTIFICATE = 0xFF03;
}

// UDS Negative Response Codes (ISO 14229)
namespace Nrc {
    constexpr uint8_t GENERAL_REJECT = 0x10;
    constexpr uint8_t SERVICE_NOT_SUPPORTED = 0x11;
    constexpr uint8_t SUB_FUNCTION_NOT_SUPPORTED = 0x12;
    constexpr uint8_t INCORRECT_MESSAGE_LENGTH = 0x13;
    constexpr uint8_t CONDITIONS_NOT_CORRECT = 0x22;
    constexpr uint8_t REQUEST_SEQUENCE_ERROR = 0x24;
    constexpr uint8_t REQUEST_OUT_OF_RANGE = 0x31;
    constexpr uint8_t SECURITY_ACCESS_DENIED = 0x33;
    constexpr uint8_t INVALID_KEY = 0x35;
    constexpr uint8_t EXCEEDED_NUMBER_OF_ATTEMPTS = 0x36;
    constexpr uint8_t REQUIRED_TIME_DELAY_NOT_EXPIRED = 0x37;
    constexpr uint8_t GENERAL_PROGRAMMING_FAILURE = 0x72;
    constexpr uint8_t RESPONSE_PENDING = 0x78;
    constexpr uint8_t BUSY_REPEAT_REQUEST = 0x21;
    constexpr uint8_t SUB_FUNCTION_NOT_SUPPORTED_IN_SESSION = 0x7E;
    constexpr uint8_t SERVICE_NOT_SUPPORTED_IN_SESSION = 0x7F;
}

// Timing parameters (milliseconds)
namespace Timing {
    // Default values (can be overridden by configuration)
    constexpr uint32_t P2_DEFAULT = 5000;     // P2 client default (5s per ISO 14229)
    constexpr uint32_t P2_STAR = 5000;       // P2* client (responsePending)
    constexpr uint32_t S3_DEFAULT = 5000;    // S3 session timeout
    
    // Configuration keys
    constexpr const char* P2_CONFIG_KEY = "timing.p2";
    constexpr const char* P2_STAR_CONFIG_KEY = "timing.p2_star";
    constexpr const char* S3_CONFIG_KEY = "timing.s3";
}

// Transport types
enum class TransportType : uint8_t {
    DOIP = 0,
    DO_CAN = 1
};

// Security access attempt limits
namespace SecurityConfig {
    constexpr uint32_t MAX_ATTEMPTS = 3;
    constexpr uint32_t LOCKOUT_DURATION_MS = 10000;  // 10 seconds
    
    // Configuration keys
    constexpr const char* MAX_ATTEMPTS_CONFIG_KEY = "security.max_attempts";
    constexpr const char* LOCKOUT_DURATION_CONFIG_KEY = "security.lockout_duration_ms";
}

// Configuration helper functions
namespace ConfigHelper {
    // Get timing value from config with fallback to default
    inline uint32_t get_timing_value(std::shared_ptr<const hwyz::config::ImmutableConfigView> config,
                                     const char* key, uint32_t default_value) {
        if (config && config->has(key)) {
            return static_cast<uint32_t>(config->getInt(key, static_cast<int>(default_value)));
        }
        return default_value;
    }
    
    // Get security config value from config with fallback to default
    inline uint32_t get_security_config(std::shared_ptr<const hwyz::config::ImmutableConfigView> config,
                                        const char* key, uint32_t default_value) {
        if (config && config->has(key)) {
            return static_cast<uint32_t>(config->getInt(key, static_cast<int>(default_value)));
        }
        return default_value;
    }
}

} // namespace diag
} // namespace tbox
