#pragma once

#include "sec_interface.h"
#include "tbox/sec/client.h"
#include <chrono>
#include <memory>
#include <mutex>

namespace tbox {
namespace diag {

class SecIpcAdapter : public SecInterface {
public:
    explicit SecIpcAdapter(std::shared_ptr<sec::SecClient> client);
    ~SecIpcAdapter() override = default;

    bool get_seed(uint8_t level, std::vector<uint8_t>& seed) override;
    bool verify_key(uint8_t level, const std::vector<uint8_t>& key) override;

    /// 返回 SEC IPC 是否可用。
    ///
    /// 连接已断开时会尝试重连（带最小重试间隔），而不是直接报不可用：
    /// framework-ipc 的 callOnce（one-shot 方法，如 GET_SEED / VERIFY_KEY）在传输
    /// 失败后会把连接标记为断开但不会自动重连，而 SEC 服务端会关闭空闲超过
    /// common.ipc.receive_timeout_ms 的连接。若此处不重连，UDS 0x27 会在 DIAG
    /// 启动若干秒后永久失效，只能重启服务才能恢复。
    bool is_available() const override;

    bool generate_key_pair() override;
    bool get_csr(std::vector<uint8_t>& csr_der) override;
    bool submit_csr() override;
    bool inject_certificate(const std::vector<uint8_t>& cert_der) override;
    bool inject_certificate(const std::vector<uint8_t>& cert_der,
                            CertInjectFailure& failure) override;

    /// 连续重连尝试之间的最小间隔，避免 SEC 宕机时产生重连风暴。
    static constexpr std::chrono::milliseconds kReconnectMinInterval{500};

private:
    /// 确保 IPC 连接可用；已断开则按最小间隔尝试一次重连。
    bool ensure_connected() const;

    std::shared_ptr<sec::SecClient> client_;
    mutable std::mutex connect_mutex_;
    mutable std::chrono::steady_clock::time_point last_connect_attempt_{};
    mutable bool connect_attempted_ = false;
};

} // namespace diag
} // namespace tbox
