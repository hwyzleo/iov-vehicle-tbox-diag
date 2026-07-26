#pragma once

namespace tbox::diag::events {

// ============================================================
// 传输层事件
// ============================================================
constexpr const char* TRANSPORT_CONNECTED = "diag.transport.connected";
constexpr const char* TRANSPORT_CONNECTION_FAILED = "diag.transport.connection_failed";
constexpr const char* TRANSPORT_LISTENING = "diag.transport.listening";
constexpr const char* TRANSPORT_DISCONNECTED = "diag.transport.disconnected";
constexpr const char* TRANSPORT_BIND_FAILED = "diag.transport.bind_failed";
constexpr const char* TRANSPORT_SOCKET_FAILED = "diag.transport.socket_failed";
constexpr const char* TRANSPORT_LISTEN_FAILED = "diag.transport.listen_failed";
constexpr const char* TRANSPORT_ACCEPT_FAILED = "diag.transport.accept_failed";
constexpr const char* TRANSPORT_PAYLOAD_TOO_LARGE = "diag.transport.payload_too_large";
constexpr const char* TRANSPORT_HANDSHAKE_FAILED = "diag.transport.handshake_failed";
constexpr const char* TRANSPORT_HANDSHAKE_ACCEPTED = "diag.transport.handshake_accepted";
constexpr const char* TRANSPORT_ROUTING_ACTIVATED = "diag.transport.routing_activated";

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
// 服务生命周期事件
// ============================================================
constexpr const char* SERVICE_STARTING = "diag.service.starting";
constexpr const char* SERVICE_INITIALIZED = "diag.service.initialized";
constexpr const char* SERVICE_SHUTTING_DOWN = "diag.service.shutting_down";
constexpr const char* SERVICE_STOPPED = "diag.service.stopped";
constexpr const char* CONFIG_LOADED = "diag.config.loaded";

// ============================================================
// 下游连接事件
// ============================================================
constexpr const char* PROV_CONNECTED = "diag.prov.connected";
constexpr const char* PROV_CONNECTION_FAILED = "diag.prov.connection_failed";

// ============================================================
// UDS 编解码事件
// ============================================================
constexpr const char* UDS_DECODE_FAILED = "diag.uds.decode_failed";
constexpr const char* UDS_SEND_FAILED = "diag.uds.send_failed";

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
