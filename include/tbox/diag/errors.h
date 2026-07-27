#pragma once

#include <cstdint>
#include <string>

namespace tbox {
namespace diag {

/// diag_client 客户端错误码
enum class DiagClientError : uint32_t {
    SUCCESS = 0,

    // 传输层错误（framework-ipc FW-03xx 映射）
    CONNECTION_FAILED = 1001,   ///< 连接失败或传输错误
    TIMEOUT = 1002,            ///< 请求超时

    // 业务错误（DIAG-10xx 透传）
    SERVICE_ERROR = 1003,      ///< DIAG 服务端返回业务错误

    // 内部错误
    INTERNAL_ERROR = 9999,
    NOT_CONNECTED = 9998,
};

std::string error_code_to_string(DiagClientError code);

} // namespace diag
} // namespace tbox
