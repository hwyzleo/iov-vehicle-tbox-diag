#include "nrc_mapper.h"
#include <iostream>
#include <map>

namespace tbox {
namespace diag {

uint8_t NrcMapper::diag_error_to_nrc(DiagErrorCode error) {
    switch (error) {
        case DiagErrorCode::TRANSPORT_CONNECTION_FAILED:
            return Nrc::CONDITIONS_NOT_CORRECT;
        case DiagErrorCode::SESSION_STATE_NOT_ALLOWED:
            return Nrc::CONDITIONS_NOT_CORRECT;
        case DiagErrorCode::SECURITY_ACCESS_DENIED:
            return Nrc::SECURITY_ACCESS_DENIED;
        case DiagErrorCode::INVALID_REQUEST_FORMAT:
            return Nrc::REQUEST_OUT_OF_RANGE;
        case DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED:
            return Nrc::GENERAL_PROGRAMMING_FAILURE;
        case DiagErrorCode::REQUEST_TIMEOUT:
            return Nrc::RESPONSE_PENDING;
        default:
            return Nrc::GENERAL_REJECT;
    }
}

DiagErrorCode NrcMapper::prov_error_to_diag(uint32_t prov_error_code) {
    // PROV error codes: 1001-1007
    switch (prov_error_code) {
        case 1001:  // SECURITY_ACCESS_NOT_GRANTED
            return DiagErrorCode::SECURITY_ACCESS_DENIED;
        case 1002:  // INVALID_VIN_FORMAT
            return DiagErrorCode::INVALID_REQUEST_FORMAT;
        case 1003:  // VIN_WRITE_FAILED
        case 1004:  // READBACK_VERIFICATION_FAILED
        case 1005:  // VIN_CONFLICT_UNAUTHORIZED
        case 1006:  // CONFIG_WRITE_FAILED
        case 1007:  // PRODUCTION_INFO_WRITE_FAILED
            return DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED;
        default:
            return DiagErrorCode::INTERNAL_ERROR;
    }
}

std::string NrcMapper::get_nrc_description(uint8_t nrc) {
    static const std::map<uint8_t, std::string> descriptions = {
        {Nrc::GENERAL_REJECT, "generalReject"},
        {Nrc::SERVICE_NOT_SUPPORTED, "serviceNotSupported"},
        {Nrc::SUB_FUNCTION_NOT_SUPPORTED, "subFunctionNotSupported"},
        {Nrc::INCORRECT_MESSAGE_LENGTH, "incorrectMessageLength"},
        {Nrc::CONDITIONS_NOT_CORRECT, "conditionsNotCorrect"},
        {Nrc::REQUEST_SEQUENCE_ERROR, "requestSequenceError"},
        {Nrc::REQUEST_OUT_OF_RANGE, "requestOutOfRange"},
        {Nrc::SECURITY_ACCESS_DENIED, "securityAccessDenied"},
        {Nrc::INVALID_KEY, "invalidKey"},
        {Nrc::EXCEEDED_NUMBER_OF_ATTEMPTS, "exceededNumberOfAttempts"},
        {Nrc::REQUIRED_TIME_DELAY_NOT_EXPIRED, "requiredTimeDelayNotExpired"},
        {Nrc::GENERAL_PROGRAMMING_FAILURE, "generalProgrammingFailure"},
        {Nrc::RESPONSE_PENDING, "responsePending"},
        {Nrc::BUSY_REPEAT_REQUEST, "busyRepeatRequest"},
        {Nrc::SUB_FUNCTION_NOT_SUPPORTED_IN_SESSION, "subFunctionNotSupportedInActiveSession"},
        {Nrc::SERVICE_NOT_SUPPORTED_IN_SESSION, "serviceNotSupportedInActiveSession"}
    };

    auto it = descriptions.find(nrc);
    if (it != descriptions.end()) {
        return it->second;
    }
    return "unknownNRC";
}

void NrcMapper::log_diag_error(DiagErrorCode error, const std::string& context) {
    std::cerr << "DIAG Error [" << error_code_to_string(error) << "]: "
              << error_code_to_description(error) << " - " << context << std::endl;
}

} // namespace diag
} // namespace tbox
