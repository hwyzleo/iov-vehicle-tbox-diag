#pragma once

#include <cstdint>
#include <string>

namespace tbox {
namespace diag {
namespace ipc {

/// DIAG 内部 IPC method ID
///
/// 其他 TBOX 服务通过 tbox::diag_client 经 framework-ipc 调用 DIAG。
/// method_id 在 framework-ipc RequestHeader 中传输，响应 JSON 内嵌
/// `status` 字段（DIAG-10xx 业务状态码）。
enum class MethodId : uint32_t {
    GET_SERVICE_STATUS = 1,   ///< 查询 DIAG 服务状态（初始化/会话/安全状态）
    GET_VEHICLE_INFO   = 2,   ///< 查询 VIN 与绑定状态
    IS_TESTER_CONNECTED = 3,  ///< 查询外部 DTE 是否连接
};

/// DIAG 内部 IPC 默认 socket 路径
constexpr const char* DEFAULT_SOCKET_PATH = "/tmp/tbox-diag.sock";

} // namespace ipc
} // namespace diag
} // namespace tbox
