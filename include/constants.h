#pragma once

#include <cstdint>
#include <string>

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

// UDS Security Levels
namespace UdsSecurityLevel {
    constexpr uint8_t LEVEL_0 = 0x00;
    constexpr uint8_t LEVEL_27 = 0x27;
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
    constexpr uint16_t CERTIFICATE_REQUEST = 0xFF01;
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
    constexpr uint32_t P2_DEFAULT = 5000;     // P2 client default (5s per ISO 14229)
    constexpr uint32_t P2_STAR = 5000;       // P2* client (responsePending)
    constexpr uint32_t S3_DEFAULT = 5000;    // S3 session timeout
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
}

} // namespace diag
} // namespace tbox
