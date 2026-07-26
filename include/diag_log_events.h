#pragma once

namespace tbox::diag::events {

// ============================================================
// 传输层事件
// ============================================================
constexpr const char* TRANSPORT_CONNECTED = "diag.transport.connected";
constexpr const char* TRANSPORT_CONNECTION_FAILED = "diag.transport.connection_failed";

// ============================================================
// 会话事件
// ============================================================
constexpr const char* SESSION_CHANGED = "diag.session.changed";
constexpr const char* SESSION_TIMED_OUT = "diag.session.timed_out";
constexpr const char* SESSION_TESTER_REJECTED = "diag.session.tester_rejected";

// ============================================================
// 安全访问事件
// ============================================================
constexpr const char* SECURITY_ACCESS_SUCCEEDED = "diag.security_access.succeeded";
constexpr const char* SECURITY_ACCESS_FAILED = "diag.security_access.failed";

// ============================================================
// UDS 路由事件
// ============================================================
constexpr const char* UDS_REQUEST_REJECTED = "diag.uds.request.rejected";
constexpr const char* UDS_REQUEST_COMPLETED = "diag.uds.request.completed";

// ============================================================
// 下游调用事件
// ============================================================
constexpr const char* DOWNSTREAM_CALL_FAILED = "diag.downstream.call_failed";

// ============================================================
// 响应事件
// ============================================================
constexpr const char* UDS_RESPONSE_PENDING = "diag.uds.response_pending";

// ============================================================
// 字段名常量
// ============================================================
namespace fields {
    constexpr const char* TRANSPORT = "transport";
    constexpr const char* SOURCE_HASH = "source_hash";
    constexpr const char* DURATION_MS = "duration_ms";
    constexpr const char* PREVIOUS_SESSION = "previous_session";
    constexpr const char* TARGET_SESSION = "target_session";
    constexpr const char* S3_MS = "s3_ms";
    constexpr const char* ACTIVE_SESSION_ID = "active_session_id";
    constexpr const char* SECURITY_LEVEL = "security_level";
    constexpr const char* FAILURE_REASON = "failure_reason";
    constexpr const char* ATTEMPT_COUNT = "attempt_count";
    constexpr const char* DELAY_MS = "delay_ms";
    constexpr const char* SERVICE_ID = "service_id";
    constexpr const char* SUB_FUNCTION = "sub_function";
    constexpr const char* DID_OR_RID = "did_or_rid";
    constexpr const char* PAYLOAD_SIZE = "payload_size";
    constexpr const char* DOWNSTREAM = "downstream";
    constexpr const char* OPERATION = "operation";
    constexpr const char* DOWNSTREAM_ERROR_CODE = "downstream_error_code";
    constexpr const char* POSITIVE = "positive";
    constexpr const char* NRC = "nrc";
    constexpr const char* ELAPSED_MS = "elapsed_ms";
    constexpr const char* TRACE_ID = "trace_id";
    constexpr const char* REQUEST_ID = "request_id";
    constexpr const char* SESSION_ID = "session_id";
} // namespace fields

} // namespace tbox::diag::events
