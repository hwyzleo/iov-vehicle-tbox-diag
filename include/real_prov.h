#pragma once

#include "prov_interface.h"
#include "prov_client.h"
#include <chrono>
#include <memory>
#include <mutex>

namespace tbox {
namespace diag {

class RealProvAdapter : public ProvInterface {
public:
    explicit RealProvAdapter(std::shared_ptr<prov::ProvClient> client);
    ~RealProvAdapter() override = default;

    DiagErrorCode write_vin(const std::string& vin, const std::vector<uint8_t>& payload) override;
    VinReadResult read_vin() override;

    /// 返回 PROV IPC 是否可用。
    ///
    /// 连接已断开时会尝试重连（带最小重试间隔），而不是直接报不可用。
    /// 与 SecIpcAdapter 同理：PROV 服务端会关闭空闲超过
    /// common.ipc.receive_timeout_ms 的连接，而 framework-ipc 的 callOnce
    /// （非幂等方法，如 WRITE_VIN）在传输失败后只把连接标记为断开、不会自动重连。
    /// 若此处不重连，断连后所有 PROV 调用会永久返回 PROV_IPC_DISCONNECTED。
    bool is_available() const override;

    /// 强制重建连接（先 disconnect 再 connect），忽略最小重试间隔。
    bool reconnect() override;

    /// 连续重连尝试之间的最小间隔，避免 PROV 宕机时产生重连风暴。
    static constexpr std::chrono::milliseconds kReconnectMinInterval{500};

private:
    /// 确保 IPC 连接可用；已断开则按最小间隔尝试一次重连。
    bool ensure_connected() const;

    std::shared_ptr<prov::ProvClient> client_;
    mutable std::mutex connect_mutex_;
    mutable std::chrono::steady_clock::time_point last_connect_attempt_{};
    mutable bool connect_attempted_ = false;
};

} // namespace diag
} // namespace tbox
