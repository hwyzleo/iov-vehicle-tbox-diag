#include "service_dispatcher.h"
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include "diag_context.h"
#include <iostream>

namespace tbox {
namespace diag {

ServiceDispatcher::ServiceDispatcher(std::shared_ptr<ProvInterface> prov,
                                     std::shared_ptr<SecInterface> sec,
                                     std::shared_ptr<SessionManager> session_mgr,
                                     std::shared_ptr<SecurityAccess> security_access)
    : prov_(prov), sec_(sec), session_mgr_(session_mgr), security_access_(security_access) {}

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
    // 创建上下文作用域
    std::string trace_id = tbox::diag::generate_trace_id();
    std::string request_id = tbox::diag::generate_request_id();
    auto scope = tbox::diag::make_context_scope(trace_id, request_id);
    
    auto start_time = std::chrono::steady_clock::now();
    
    DiagResponse response;
    switch (request.service_id) {
        case UdsService::DIAGNOSTIC_SESSION_CONTROL:
            response = handle_session_control(request);
            break;
        case UdsService::TESTER_PRESENT:
            response = handle_tester_present(request);
            break;
        case UdsService::SECURITY_ACCESS:
            response = handle_security_access(request);
            break;
        case UdsService::ROUTINE_CONTROL:
            response = handle_routine_control(request);
            break;
        case UdsService::READ_DATA_BY_IDENTIFIER:
            response = handle_read_data_by_identifier(request);
            break;
        default:
            response = create_negative_response(request.service_id,
                                            Nrc::SERVICE_NOT_SUPPORTED,
                                            "DIAG-1004");
            break;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 记录请求完成
    tbox::diag::DiagLogAdapter::uds_router().debug(
        tbox::diag::events::UDS_REQUEST_COMPLETED,
        "UDS request completed",
        {
            tbox::fw::log::Field(tbox::diag::events::fields::SERVICE_ID, 
                tbox::fw::log::FieldValue::makeString("0x" + std::to_string(request.service_id))),
            tbox::fw::log::Field(tbox::diag::events::fields::POSITIVE, 
                tbox::fw::log::FieldValue::makeBool(response.positive)),
            tbox::fw::log::Field(tbox::diag::events::fields::NRC, 
                tbox::fw::log::FieldValue::makeString("0x" + std::to_string(response.nrc))),
            tbox::fw::log::Field(tbox::diag::events::fields::DURATION_MS, 
                tbox::fw::log::FieldValue::makeInt(duration.count()))
        }
    );
    
    return response;
}

DiagResponse ServiceDispatcher::handle_session_control(const DiagRequest& request) {
    uint8_t session_type = request.sub_function & 0x7F;
    {
        char sub_buf[16], sess_buf[16];
        snprintf(sub_buf, sizeof(sub_buf), "0x%02X", request.sub_function);
        snprintf(sess_buf, sizeof(sess_buf), "0x%02X", session_type);
        DiagLogAdapter::session().info(
            events::SESSION_CHANGED,
            "SessionControl request",
            {fw::log::Field(events::fields::SUB_FUNCTION,
                fw::log::FieldValue::makeString(sub_buf)),
             fw::log::Field(events::fields::TARGET_SESSION,
                fw::log::FieldValue::makeString(sess_buf))}
        );
    }

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
    uint8_t raw_level = request.sub_function & 0x7F;

    // TBOX-DIAG-DSN-CR-005: only level-1 sub-functions are supported after
    // masking suppressPosRspMsgIndicationBit (bit7): 0x01=requestSeed,
    // 0x02=sendKey. Anything else -> NRC 0x12 subFunctionNotSupported.
    if (raw_level != UdsSecurityLevel::LEVEL_1 &&
        raw_level != (UdsSecurityLevel::LEVEL_1 + 1)) {
        return create_negative_response(UdsService::SECURITY_ACCESS,
                                        Nrc::SUB_FUNCTION_NOT_SUPPORTED,
                                        "DIAG-1004");
    }

    bool is_request_seed = (raw_level & 0x01) != 0;

    if (is_request_seed) {
        std::vector<uint8_t> seed;
        {
            char sub_buf[16];
            snprintf(sub_buf, sizeof(sub_buf), "0x%02X", request.sub_function);
            DiagLogAdapter::session().info(
                events::SECURITY_ACCESS_SUCCEEDED,
                "Security access request_seed",
                {fw::log::Field(events::fields::SUB_FUNCTION,
                    fw::log::FieldValue::makeString(sub_buf))}
            );
        }
        auto result = security_access_->request_seed(raw_level, seed);
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
        auto result = security_access_->send_key(raw_level, request.payload);
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
    {
        char rid_buf[16];
        snprintf(rid_buf, sizeof(rid_buf), "0x%04X", request.did_or_rid);
        DiagLogAdapter::uds_router().info(
            "diag.uds.routine_control",
            "RoutineControl request",
            {fw::log::Field(events::fields::DID_OR_RID,
                fw::log::FieldValue::makeString(rid_buf)),
             fw::log::Field(events::fields::PAYLOAD_SIZE,
                fw::log::FieldValue::makeInt(static_cast<int64_t>(request.payload.size())))}
        );
    }

    uint16_t rid = request.did_or_rid;

    // 检查会话类型（证书相关操作只在 Programming Session 下可用）
    if (rid == Rid::GENERATE_KEY_PAIR || rid == Rid::READ_CSR || rid == Rid::INJECT_CERTIFICATE) {
        auto current_session = session_mgr_->get_session();
        {
            char cur_buf[16], exp_buf[16];
            snprintf(cur_buf, sizeof(cur_buf), "0x%02X", current_session.session_type);
            snprintf(exp_buf, sizeof(exp_buf), "0x%02X", static_cast<int>(SessionType::PROGRAMMING));
            DiagLogAdapter::uds_router().debug(
                "diag.uds.certificate_session_check",
                "Certificate RID session check",
                {fw::log::Field("current_session",
                    fw::log::FieldValue::makeString(cur_buf)),
                 fw::log::Field("expected_session",
                    fw::log::FieldValue::makeString(exp_buf))}
            );
        }
        if (current_session.session_type != SessionType::PROGRAMMING) {
            return create_negative_response(UdsService::ROUTINE_CONTROL,
                                            Nrc::SERVICE_NOT_SUPPORTED_IN_SESSION,
                                            error_code_to_string(DiagErrorCode::SESSION_STATE_NOT_ALLOWED));
        }
    }

    // 新路由
    if (rid == Rid::GENERATE_KEY_PAIR) {
        return handle_generate_key_pair(request);
    }
    if (rid == Rid::READ_CSR) {
        return handle_read_csr(request);
    }
    if (rid == Rid::INJECT_CERTIFICATE) {
        return handle_inject_certificate(request);
    }

    // 检查安全访问（TBOX-DIAG-DSN-CR-005: 统一 level-1 状态键 0x01）
    if (!security_access_->is_unlocked(UdsSecurityLevel::LEVEL_1)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::SECURITY_ACCESS_DENIED,
                                        error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED));
    }

    // 检查 PROV 可用性
    if (!prov_ || !prov_->is_available()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(DiagErrorCode::PROV_UNAVAILABLE));
    }

    if (rid == Rid::WRITE_VIN_ROUTINE) {
        // Extract VIN from payload (17 bytes)
        if (request.payload.size() < 17) {
            DiagLogAdapter::uds_router().warn(
                events::UDS_REQUEST_REJECTED,
                "RoutineControl: payload too small for VIN",
                {fw::log::Field(events::fields::PAYLOAD_SIZE,
                    fw::log::FieldValue::makeInt(static_cast<int64_t>(request.payload.size()))),
                 fw::log::Field("required_size",
                    fw::log::FieldValue::makeInt(17))}
            );
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

DiagResponse ServiceDispatcher::handle_generate_key_pair(const DiagRequest& request) {
    {
        char rid_buf[16];
        snprintf(rid_buf, sizeof(rid_buf), "0x%04X", request.did_or_rid);
        DiagLogAdapter::uds_router().info(
            "diag.uds.generate_key_pair",
            "GenerateKeyPair request",
            {fw::log::Field(events::fields::DID_OR_RID,
                fw::log::FieldValue::makeString(rid_buf))}
        );
    }

    // 检查安全访问（TBOX-DIAG-DSN-CR-005: 统一 level-1 状态键 0x01）
    if (!security_access_->is_unlocked(UdsSecurityLevel::LEVEL_1)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::SECURITY_ACCESS_DENIED,
                                        error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED));
    }

    // 检查 SEC 服务可用性
    if (!sec_ || !sec_->is_available()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(DiagErrorCode::SEC_UNAVAILABLE));
    }

    // 调用 SEC 服务生成密钥对
    if (!sec_->generate_key_pair()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::GENERAL_PROGRAMMING_FAILURE,
                                        error_code_to_string(DiagErrorCode::CERT_GENERATION_FAILED));
    }

    // 返回正响应
    std::vector<uint8_t> rid_echo = {
        static_cast<uint8_t>((request.did_or_rid >> 8) & 0xFF),
        static_cast<uint8_t>(request.did_or_rid & 0xFF)
    };
    return create_positive_response(UdsService::ROUTINE_CONTROL,
                                    request.sub_function, rid_echo);
}

DiagResponse ServiceDispatcher::handle_read_csr(const DiagRequest& request) {
    {
        char rid_buf[16];
        snprintf(rid_buf, sizeof(rid_buf), "0x%04X", request.did_or_rid);
        DiagLogAdapter::uds_router().info(
            "diag.uds.read_csr",
            "ReadCSR request",
            {fw::log::Field(events::fields::DID_OR_RID,
                fw::log::FieldValue::makeString(rid_buf))}
        );
    }

    // 检查安全访问（TBOX-DIAG-DSN-CR-005: 统一 level-1 状态键 0x01）
    if (!security_access_->is_unlocked(UdsSecurityLevel::LEVEL_1)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::SECURITY_ACCESS_DENIED,
                                        error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED));
    }

    // 检查 SEC 服务可用性
    if (!sec_ || !sec_->is_available()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(DiagErrorCode::SEC_UNAVAILABLE));
    }

    // 获取 CSR
    std::vector<uint8_t> csr_der;
    bool result = sec_->get_csr(csr_der);
    DiagLogAdapter::downstream().debug(
        "diag.downstream.get_csr_result",
        "get_csr result",
        {fw::log::Field("success",
            fw::log::FieldValue::makeBool(result)),
         fw::log::Field("csr_size",
            fw::log::FieldValue::makeInt(static_cast<int64_t>(csr_der.size())))}
    );
    if (!result) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::GENERAL_PROGRAMMING_FAILURE,
                                        error_code_to_string(DiagErrorCode::CSR_CREATION_FAILED));
    }

    // 返回正响应（包含CSR数据）
    std::vector<uint8_t> response_data = {
        static_cast<uint8_t>((request.did_or_rid >> 8) & 0xFF),
        static_cast<uint8_t>(request.did_or_rid & 0xFF)
    };
    response_data.insert(response_data.end(), csr_der.begin(), csr_der.end());
    DiagLogAdapter::response().debug(
        "diag.uds.read_csr_response",
        "ReadCSR response built",
        {fw::log::Field(events::fields::PAYLOAD_SIZE,
            fw::log::FieldValue::makeInt(static_cast<int64_t>(response_data.size())))}
    );
    return create_positive_response(UdsService::ROUTINE_CONTROL,
                                    request.sub_function, response_data);
}

DiagResponse ServiceDispatcher::handle_inject_certificate(const DiagRequest& request) {
    {
        char rid_buf[16];
        snprintf(rid_buf, sizeof(rid_buf), "0x%04X", request.did_or_rid);
        DiagLogAdapter::uds_router().info(
            "diag.uds.inject_certificate",
            "InjectCertificate request",
            {fw::log::Field(events::fields::DID_OR_RID,
                fw::log::FieldValue::makeString(rid_buf)),
             fw::log::Field(events::fields::PAYLOAD_SIZE,
                fw::log::FieldValue::makeInt(static_cast<int64_t>(request.payload.size())))}
        );
    }

    // 检查安全访问（TBOX-DIAG-DSN-CR-005: 统一 level-1 状态键 0x01）
    if (!security_access_->is_unlocked(UdsSecurityLevel::LEVEL_1)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::SECURITY_ACCESS_DENIED,
                                        error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED));
    }

    // 检查 SEC 服务可用性
    if (!sec_ || !sec_->is_available()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(DiagErrorCode::SEC_UNAVAILABLE));
    }

    // 从请求中提取证书数据
    if (request.payload.empty()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::INCORRECT_MESSAGE_LENGTH,
                                        error_code_to_string(DiagErrorCode::INVALID_REQUEST_FORMAT));
    }

    // 注入证书
    if (!sec_->inject_certificate(request.payload)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::GENERAL_PROGRAMMING_FAILURE,
                                        error_code_to_string(DiagErrorCode::CERT_INJECTION_FAILED));
    }

    // 返回正响应
    std::vector<uint8_t> rid_echo = {
        static_cast<uint8_t>((request.did_or_rid >> 8) & 0xFF),
        static_cast<uint8_t>(request.did_or_rid & 0xFF)
    };
    return create_positive_response(UdsService::ROUTINE_CONTROL,
                                    request.sub_function, rid_echo);
}

DiagResponse ServiceDispatcher::handle_read_data_by_identifier(const DiagRequest& request) {
    uint16_t did = request.did_or_rid;
    {
        char did_buf[16], src_buf[16];
        snprintf(did_buf, sizeof(did_buf), "0x%04X", did);
        snprintf(src_buf, sizeof(src_buf), "0x%04X", request.source_address);
        DiagLogAdapter::uds_router().info(
            "diag.uds.read_did",
            "ReadDID request",
            {fw::log::Field(events::fields::DID_OR_RID,
                fw::log::FieldValue::makeString(did_buf)),
             fw::log::Field("source_address",
                fw::log::FieldValue::makeString(src_buf)),
             fw::log::Field(events::fields::TRANSPORT,
                fw::log::FieldValue::makeInt(static_cast<int64_t>(request.transport)))}
        );
    }

    if (did == Did::VIN || did == Did::BINDING_STATE) {
        // Check PROV availability
        if (!prov_ || !prov_->is_available()) {
            DiagLogAdapter::downstream().warn(
                events::DOWNSTREAM_CALL_FAILED,
                "ReadDID: PROV unavailable",
                {fw::log::Field(events::fields::DOWNSTREAM,
                    fw::log::FieldValue::makeString("prov"))}
            );
            return create_negative_response(UdsService::READ_DATA_BY_IDENTIFIER,
                                            Nrc::CONDITIONS_NOT_CORRECT,
                                            error_code_to_string(DiagErrorCode::PROV_UNAVAILABLE));
        }

        DiagLogAdapter::downstream().debug(
            "diag.downstream.read_vin",
            "Calling prov->read_vin()"
        );
        auto read_result = prov_->read_vin();
        DiagLogAdapter::downstream().debug(
            "diag.downstream.read_vin_result",
            "read_vin returned",
            {fw::log::Field("valid",
                fw::log::FieldValue::makeBool(read_result.valid))}
        );
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

        DiagLogAdapter::response().debug(
            "diag.uds.read_did_response",
            "ReadDID positive response",
            {fw::log::Field(events::fields::PAYLOAD_SIZE,
                fw::log::FieldValue::makeInt(static_cast<int64_t>(data.size())))}
        );
        return create_positive_response(UdsService::READ_DATA_BY_IDENTIFIER, 0, data);
    }

    DiagLogAdapter::uds_router().warn(
        events::UDS_REQUEST_REJECTED,
        "ReadDID: DID out of range",
        {fw::log::Field(events::fields::NRC,
            fw::log::FieldValue::makeString("0x31"))}
    );
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
