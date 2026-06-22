#include "error_codes.h"
#include <map>

namespace tbox {
namespace diag {

static const std::map<DiagErrorCode, std::string> error_code_strings = {
    {DiagErrorCode::SUCCESS, "SUCCESS"},
    {DiagErrorCode::TRANSPORT_CONNECTION_FAILED, "DIAG-1001"},
    {DiagErrorCode::SESSION_STATE_NOT_ALLOWED, "DIAG-1002"},
    {DiagErrorCode::SECURITY_ACCESS_DENIED, "DIAG-1003"},
    {DiagErrorCode::INVALID_REQUEST_FORMAT, "DIAG-1004"},
    {DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED, "DIAG-1005"},
    {DiagErrorCode::REQUEST_TIMEOUT, "DIAG-1006"},
    {DiagErrorCode::CERT_GENERATION_FAILED, "DIAG-1007"},
    {DiagErrorCode::CSR_CREATION_FAILED, "DIAG-1008"},
    {DiagErrorCode::CERT_SUBMISSION_FAILED, "DIAG-1009"},
    {DiagErrorCode::CERT_INJECTION_FAILED, "DIAG-1010"},
    {DiagErrorCode::INTERNAL_ERROR, "INTERNAL-9999"},
    {DiagErrorCode::NOT_INITIALIZED, "INTERNAL-9998"},
    {DiagErrorCode::ROUTING_NOT_FOUND, "INTERNAL-9997"},
    {DiagErrorCode::SEC_UNAVAILABLE, "INTERNAL-9996"},
    {DiagErrorCode::PROV_UNAVAILABLE, "INTERNAL-9995"}
};

static const std::map<DiagErrorCode, std::string> error_code_descriptions = {
    {DiagErrorCode::SUCCESS, "操作成功"},
    {DiagErrorCode::TRANSPORT_CONNECTION_FAILED, "传输层连接/会话建立失败"},
    {DiagErrorCode::SESSION_STATE_NOT_ALLOWED, "会话状态不允许该请求"},
    {DiagErrorCode::SECURITY_ACCESS_DENIED, "安全访问未解锁/鉴权失败"},
    {DiagErrorCode::INVALID_REQUEST_FORMAT, "请求格式/参数非法"},
    {DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED, "下游服务(PROV)执行失败"},
    {DiagErrorCode::REQUEST_TIMEOUT, "请求处理超时"},
    {DiagErrorCode::CERT_GENERATION_FAILED, "密钥对生成失败"},
    {DiagErrorCode::CSR_CREATION_FAILED, "CSR创建失败"},
    {DiagErrorCode::CERT_SUBMISSION_FAILED, "证书提交失败"},
    {DiagErrorCode::CERT_INJECTION_FAILED, "证书注入失败"},
    {DiagErrorCode::INTERNAL_ERROR, "内部错误"},
    {DiagErrorCode::NOT_INITIALIZED, "服务未初始化"},
    {DiagErrorCode::ROUTING_NOT_FOUND, "路由条目未找到"},
    {DiagErrorCode::SEC_UNAVAILABLE, "SEC服务不可用"},
    {DiagErrorCode::PROV_UNAVAILABLE, "PROV服务不可用"}
};

std::string error_code_to_string(DiagErrorCode code) {
    auto it = error_code_strings.find(code);
    if (it != error_code_strings.end()) {
        return it->second;
    }
    return "UNKNOWN";
}

std::string error_code_to_description(DiagErrorCode code) {
    auto it = error_code_descriptions.find(code);
    if (it != error_code_descriptions.end()) {
        return it->second;
    }
    return "未知错误";
}

DiagErrorCode nrc_to_diag_error(uint8_t nrc) {
    switch (nrc) {
        case 0x22: // conditionsNotCorrect
        case 0x21: // busyRepeatRequest
            return DiagErrorCode::SESSION_STATE_NOT_ALLOWED;
        case 0x33: // securityAccessDenied
        case 0x35: // invalidKey
            return DiagErrorCode::SECURITY_ACCESS_DENIED;
        case 0x13: // incorrectMessageLength
        case 0x31: // requestOutOfRange
            return DiagErrorCode::INVALID_REQUEST_FORMAT;
        case 0x72: // generalProgrammingFailure
            return DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED;
        case 0x78: // responsePending
            return DiagErrorCode::REQUEST_TIMEOUT;
        default:
            return DiagErrorCode::INTERNAL_ERROR;
    }
}

} // namespace diag
} // namespace tbox
