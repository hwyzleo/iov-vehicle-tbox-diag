#include "service_dispatcher.h"
#include <iostream>

namespace tbox {
namespace diag {

ServiceDispatcher::ServiceDispatcher(std::shared_ptr<ProvInterface> prov,
                                     std::shared_ptr<SessionManager> session_mgr,
                                     std::shared_ptr<SecurityAccess> security_access)
    : prov_(prov), session_mgr_(session_mgr), security_access_(security_access) {}

void ServiceDispatcher::register_route(uint8_t service_id, uint16_t did_or_rid,
                                        const std::string& downstream, bool requires_unlock) {
    RoutingEntry entry;
    entry.service_id = service_id;
    entry.did_or_rid = did_or_rid;
    entry.downstream = downstream;
    entry.requires_unlock = requires_unlock;
    routing_table_.push_back(entry);
}

DiagResponse ServiceDispatcher::dispatch(const DiagRequest& request) {
    switch (request.service_id) {
        case UdsService::DIAGNOSTIC_SESSION_CONTROL:
            return handle_session_control(request);
        case UdsService::TESTER_PRESENT:
            return handle_tester_present(request);
        case UdsService::SECURITY_ACCESS:
            return handle_security_access(request);
        case UdsService::ROUTINE_CONTROL:
            return handle_routine_control(request);
        case UdsService::READ_DATA_BY_IDENTIFIER:
            return handle_read_data_by_identifier(request);
        default:
            return create_negative_response(request.service_id,
                                            Nrc::SERVICE_NOT_SUPPORTED,
                                            "DIAG-1004");
    }
}

DiagResponse ServiceDispatcher::handle_session_control(const DiagRequest& request) {
    uint8_t session_type = request.sub_function & 0x7F;

    auto result = session_mgr_->switch_session(session_type, request.source_address,
                                                request.transport);
    if (result != DiagErrorCode::SUCCESS) {
        uint8_t nrc = (result == DiagErrorCode::SESSION_STATE_NOT_ALLOWED)
                      ? Nrc::CONDITIONS_NOT_CORRECT
                      : Nrc::INCORRECT_MESSAGE_LENGTH;
        return create_negative_response(UdsService::DIAGNOSTIC_SESSION_CONTROL, nrc,
                                        error_code_to_string(result));
    }

    // ISO 14229: 0x50 positive response = [session_type] [P2(2)] [P2*(2)]
    // P2 in ms, P2* in 10ms units
    uint16_t p2_server = static_cast<uint16_t>(Timing::P2_DEFAULT);
    uint16_t p2_star_server = static_cast<uint16_t>(Timing::P2_STAR / 10);
    std::vector<uint8_t> data = {
        static_cast<uint8_t>((p2_server >> 8) & 0xFF),
        static_cast<uint8_t>(p2_server & 0xFF),
        static_cast<uint8_t>((p2_star_server >> 8) & 0xFF),
        static_cast<uint8_t>(p2_star_server & 0xFF)
    };
    return create_positive_response(UdsService::DIAGNOSTIC_SESSION_CONTROL,
                                    session_type, data);
}

DiagResponse ServiceDispatcher::handle_tester_present(const DiagRequest& request) {
    auto result = session_mgr_->handle_tester_present(request.source_address);
    if (result != DiagErrorCode::SUCCESS) {
        return create_negative_response(UdsService::TESTER_PRESENT,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(result));
    }

    return create_positive_response(UdsService::TESTER_PRESENT,
                                    request.sub_function, {});
}

DiagResponse ServiceDispatcher::handle_security_access(const DiagRequest& request) {
    bool is_request_seed = (request.sub_function & 0x80) == 0;
    // ISO 14229: requestSeed uses odd level, sendKey uses even level (seed level + 1)
    // Normalize to the seed level (odd) for state lookup
    uint8_t raw_level = request.sub_function & 0x7F;
    uint8_t level = is_request_seed ? raw_level : (raw_level - 1);

    if (is_request_seed) {
        std::vector<uint8_t> seed;
        std::cout << "[DIAG] request_seed sub_function=0x" << std::hex
                  << (int)request.sub_function << std::endl;
        auto result = security_access_->request_seed(level, seed);
        if (result != DiagErrorCode::SUCCESS) {
            uint8_t nrc = Nrc::SECURITY_ACCESS_DENIED;
            if (result == DiagErrorCode::SEC_UNAVAILABLE) {
                nrc = Nrc::CONDITIONS_NOT_CORRECT;
            }
            return create_negative_response(UdsService::SECURITY_ACCESS, nrc,
                                            error_code_to_string(result));
        }
        return create_positive_response(UdsService::SECURITY_ACCESS,
                                        request.sub_function, seed);
    } else {
        auto result = security_access_->send_key(level, request.payload);
        if (result != DiagErrorCode::SUCCESS) {
            uint8_t nrc = Nrc::INVALID_KEY;
            if (result == DiagErrorCode::SEC_UNAVAILABLE) {
                nrc = Nrc::CONDITIONS_NOT_CORRECT;
            }
            return create_negative_response(UdsService::SECURITY_ACCESS, nrc,
                                            error_code_to_string(result));
        }
        return create_positive_response(UdsService::SECURITY_ACCESS,
                                        request.sub_function, {});
    }
}

DiagResponse ServiceDispatcher::handle_routine_control(const DiagRequest& request) {
    std::cout << "[DIAG] RoutineControl rid=0x" << std::hex << request.did_or_rid
              << " payload_size=" << std::dec << request.payload.size() << std::endl;
    // Check security access
    if (!security_access_->is_unlocked(UdsSecurityLevel::LEVEL_27)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::SECURITY_ACCESS_DENIED,
                                        error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED));
    }

    // Check PROV availability
    if (!prov_ || !prov_->is_available()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(DiagErrorCode::PROV_UNAVAILABLE));
    }

    uint16_t rid = request.did_or_rid;
    if (rid == Rid::WRITE_VIN_ROUTINE) {
        // Extract VIN from payload (17 bytes)
        if (request.payload.size() < 17) {
            std::cout << "[DIAG] RoutineControl NRC0x13: payload_size=" << std::dec << request.payload.size()
                      << " (need >= 17)" << std::endl;
            return create_negative_response(UdsService::ROUTINE_CONTROL,
                                            Nrc::INCORRECT_MESSAGE_LENGTH,
                                            error_code_to_string(DiagErrorCode::INVALID_REQUEST_FORMAT));
        }

        std::string vin(request.payload.begin(), request.payload.begin() + 17);
        std::vector<uint8_t> extra_payload(request.payload.begin() + 17, request.payload.end());

        auto result = prov_->write_vin(vin, extra_payload);
        if (result != DiagErrorCode::SUCCESS) {
            uint8_t nrc = Nrc::GENERAL_PROGRAMMING_FAILURE;
            return create_negative_response(UdsService::ROUTINE_CONTROL, nrc,
                                            error_code_to_string(result));
        }

        // ISO 14229: positive response = [controlType] [routineId(2)] [statusRecord...]
        std::vector<uint8_t> rid_echo = {
            static_cast<uint8_t>((request.did_or_rid >> 8) & 0xFF),
            static_cast<uint8_t>(request.did_or_rid & 0xFF)
        };
        return create_positive_response(UdsService::ROUTINE_CONTROL,
                                        request.sub_function, rid_echo);
    }

    return create_negative_response(UdsService::ROUTINE_CONTROL,
                                    Nrc::REQUEST_OUT_OF_RANGE,
                                    "DIAG-1004");
}

DiagResponse ServiceDispatcher::handle_read_data_by_identifier(const DiagRequest& request) {
    uint16_t did = request.did_or_rid;

    if (did == Did::VIN || did == Did::BINDING_STATE) {
        // Check PROV availability
        if (!prov_ || !prov_->is_available()) {
            return create_negative_response(UdsService::READ_DATA_BY_IDENTIFIER,
                                            Nrc::CONDITIONS_NOT_CORRECT,
                                            error_code_to_string(DiagErrorCode::PROV_UNAVAILABLE));
        }

        auto read_result = prov_->read_vin();
        if (!read_result.valid) {
            return create_negative_response(UdsService::READ_DATA_BY_IDENTIFIER,
                                            Nrc::CONDITIONS_NOT_CORRECT,
                                            error_code_to_string(DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED));
        }

        std::vector<uint8_t> data;
        // DID echo
        data.push_back(static_cast<uint8_t>((did >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(did & 0xFF));

        if (did == Did::VIN) {
            data.insert(data.end(), read_result.vin.begin(), read_result.vin.end());
        } else {
            data.insert(data.end(), read_result.bind_state.begin(), read_result.bind_state.end());
        }

        return create_positive_response(UdsService::READ_DATA_BY_IDENTIFIER, 0, data);
    }

    return create_negative_response(UdsService::READ_DATA_BY_IDENTIFIER,
                                    Nrc::REQUEST_OUT_OF_RANGE,
                                    "DIAG-1004");
}

DiagResponse ServiceDispatcher::create_positive_response(uint8_t service_id, uint8_t sub_function,
                                                          const std::vector<uint8_t>& data) {
    DiagResponse response;
    response.positive = true;
    response.service_id = service_id + 0x40;  // Positive response SID
    response.sub_function = sub_function;
    response.payload = data;
    response.nrc = 0;
    return response;
}

DiagResponse ServiceDispatcher::create_negative_response(uint8_t service_id, uint8_t nrc,
                                                          const std::string& diag_error_code) {
    DiagResponse response;
    response.positive = false;
    response.service_id = 0x7F;  // Negative response SID
    response.sub_function = service_id;
    response.nrc = nrc;
    response.diag_error_code = diag_error_code;
    return response;
}

bool ServiceDispatcher::check_security_required(uint8_t service_id, uint16_t did_or_rid) {
    for (const auto& entry : routing_table_) {
        if (entry.service_id == service_id && entry.did_or_rid == did_or_rid) {
            return entry.requires_unlock;
        }
    }
    return false;
}

} // namespace diag
} // namespace tbox
