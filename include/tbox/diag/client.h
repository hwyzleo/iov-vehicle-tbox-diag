#pragma once

#include <string>
#include <memory>
#include "tbox/diag/types.h"
#include "tbox/diag/errors.h"

namespace tbox {
namespace diag {

/// DIAG 客户端 facade
///
/// 调用方（其他 TBOX 服务）通过此接口与 DIAG daemon 交互。
/// IPC 传输细节封装在库内部，对调用方不可见。
/// 内部使用 framework-ipc Client + DiagRetryPolicy。
class DiagClient {
public:
    DiagClient(const std::string& socket_path = "/tmp/tbox-diag.sock");
    ~DiagClient();

    DiagClient(const DiagClient&) = delete;
    DiagClient& operator=(const DiagClient&) = delete;

    // 连接到 DIAG 服务
    bool connect();
    void disconnect();
    bool is_connected() const;

    // 查询 DIAG 服务状态
    DiagClientError get_service_status(DiagServiceStatus& status);

    // 查询车辆信息（VIN 与绑定状态，经 DIAG 路由至 PROV）
    DiagClientError get_vehicle_info(VehicleInfo& info);

    // 查询外部诊断仪（DTE）是否连接
    DiagClientError is_tester_connected(bool& connected);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace diag
} // namespace tbox
