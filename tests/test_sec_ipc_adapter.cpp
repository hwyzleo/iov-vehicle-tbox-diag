// SEC IPC 适配器重连回归测试。
//
// 背景（实车故障）：SEC 服务端会关闭空闲超过 common.ipc.receive_timeout_ms
// （默认 60s）的 IPC 连接。GET_SEED / VERIFY_KEY 被 SecRetryPolicy 归为 kOneShot，
// 走 framework-ipc 的 callOnce —— 传输失败后只把连接标记为断开，不会自动重连。
// 修复前 SecIpcAdapter::is_available() 只是 client_->is_connected() 的只读查询，
// 且所有方法都硬门禁在它上面，于是一旦断连就永久不可用：UDS 0x27 在 DIAG 启动
// 约 60 秒后永久返回 NRC 0x22，只能重启 tbox-diag 才能恢复。

#include "sec_ipc_adapter.h"

#include "ipc.h"
#include "ipc_protocol.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <unistd.h>

namespace tbox {
namespace diag {
namespace testing {

namespace {

/// 最小 SEC IPC 服务端桩：只需要能 accept 连接。
class StubSecServer {
public:
    explicit StubSecServer(std::string socket_path)
        : socket_path_(std::move(socket_path)) {}

    ~StubSecServer() { stop(); }

    bool start() {
        server_ = std::make_unique<fw::ipc::Server>(socket_path_);
        return server_->start(
            [](uint32_t, std::string_view, int) -> std::string {
                // SecClient 只读取 JSON 里的 "status" 字段作为业务状态码。
                return R"({"status":0})";
            });
    }

    void stop() {
        if (server_) {
            server_->stop();
            server_.reset();
        }
    }

private:
    std::string socket_path_;
    std::unique_ptr<fw::ipc::Server> server_;
};

std::string make_socket_path() {
    return "/tmp/tbox-sec-test-" + std::to_string(::getpid()) + "-" +
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
           ".sock";
}

}  // namespace

class SecIpcAdapterReconnectTest : public ::testing::Test {
protected:
    void SetUp() override {
        socket_path_ = make_socket_path();
        server_ = std::make_unique<StubSecServer>(socket_path_);
        ASSERT_TRUE(server_->start());

        client_ = std::make_shared<sec::SecClient>(socket_path_);
        adapter_ = std::make_unique<SecIpcAdapter>(client_);
    }

    void TearDown() override {
        adapter_.reset();
        client_.reset();
        server_.reset();
        ::unlink(socket_path_.c_str());
    }

    std::string socket_path_;
    std::unique_ptr<StubSecServer> server_;
    std::shared_ptr<sec::SecClient> client_;
    std::unique_ptr<SecIpcAdapter> adapter_;
};

TEST_F(SecIpcAdapterReconnectTest, AvailableAfterInitialConnect) {
    ASSERT_TRUE(client_->connect());
    EXPECT_TRUE(adapter_->is_available());
}

// 核心回归：模拟服务端关闭空闲连接后客户端被标记为断开的状态。
// 修复前 is_available() 会永久返回 false；修复后应自动重连。
TEST_F(SecIpcAdapterReconnectTest, ReconnectsAfterConnectionDropped) {
    ASSERT_TRUE(client_->connect());
    ASSERT_TRUE(adapter_->is_available());

    // 等价于 callOnce 传输失败后 framework 置 m_connected = false 的状态。
    client_->disconnect();
    ASSERT_FALSE(client_->is_connected()) << "前置条件：连接应处于断开状态";

    EXPECT_TRUE(adapter_->is_available())
        << "SEC IPC 断连后未重连：0x27 将永久不可用";
    EXPECT_TRUE(client_->is_connected());
}

// 反复断连都应能恢复，而不是只恢复一次。
TEST_F(SecIpcAdapterReconnectTest, ReconnectsRepeatedly) {
    ASSERT_TRUE(client_->connect());

    for (int i = 0; i < 3; ++i) {
        client_->disconnect();
        ASSERT_FALSE(client_->is_connected()) << "iteration " << i;
        // 重连有最小间隔节流，等待其过期后再尝试。
        std::this_thread::sleep_for(SecIpcAdapter::kReconnectMinInterval +
                                    std::chrono::milliseconds(50));
        EXPECT_TRUE(adapter_->is_available()) << "iteration " << i;
    }
}

TEST_F(SecIpcAdapterReconnectTest, UnavailableWhenServerIsGone) {
    ASSERT_TRUE(client_->connect());
    ASSERT_TRUE(adapter_->is_available());

    server_->stop();
    client_->disconnect();

    // 服务端不在了，重连必然失败，但不得挂起或崩溃。
    EXPECT_FALSE(adapter_->is_available());
}

TEST_F(SecIpcAdapterReconnectTest, ThrottlesReconnectAttemptsWhileServerIsDown) {
    server_->stop();
    client_->disconnect();

    // 首次尝试会真正发起 connect 并失败；紧随其后的调用应被最小间隔节流，
    // 因此整批调用必须远快于「每次都真正建链」的耗时。
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; ++i) {
        EXPECT_FALSE(adapter_->is_available());
    }
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, SecIpcAdapter::kReconnectMinInterval * 10)
        << "重连未被节流，存在重连风暴风险";
}

TEST(SecIpcAdapterTest, UnavailableWithNullClient) {
    SecIpcAdapter adapter(nullptr);
    EXPECT_FALSE(adapter.is_available());
}

}  // namespace testing
}  // namespace diag
}  // namespace tbox
