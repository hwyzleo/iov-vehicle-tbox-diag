// PROV IPC 适配器重连回归测试。
//
// 背景：ProvInterface 声明了 reconnect()，但 DIAG 代码里从来没有调用过它，而
// RealProvAdapter::is_available() 只是 client_->is_connected() 的只读查询，
// 且 write_vin / read_vin 都硬门禁在它上面。PROV 之所以一直没暴露问题，是因为
// ProvClient 所有方法都走 framework-ipc 的 call()（内部失败后会重连并重试一次），
// 陈旧连接能被自动救回、m_connected 不会翻成 false。
//
// 但只要两次尝试都失败（例如 PROV 服务重启、socket 短时不可用），
// m_connected 就会被置为 false，此后 is_available() 永久返回 false，
// 所有 PROV 调用恒返回 PROV_IPC_DISCONNECTED，只能重启 tbox-diag —— 与
// SecIpcAdapter 修复前完全相同的故障模式。

#include "real_prov.h"

#include "ipc.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <unistd.h>

namespace tbox {
namespace diag {
namespace testing {

namespace {

/// 最小 PROV IPC 服务端桩：只需要能 accept 连接。
class StubProvServer {
public:
    explicit StubProvServer(std::string socket_path)
        : socket_path_(std::move(socket_path)) {}

    ~StubProvServer() { stop(); }

    bool start() {
        server_ = std::make_unique<fw::ipc::Server>(socket_path_);
        return server_->start(
            [](uint32_t, std::string_view, int) -> std::string {
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
    return "/tmp/tbox-prov-test-" + std::to_string(::getpid()) + "-" +
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
           ".sock";
}

}  // namespace

class RealProvAdapterReconnectTest : public ::testing::Test {
protected:
    void SetUp() override {
        socket_path_ = make_socket_path();
        server_ = std::make_unique<StubProvServer>(socket_path_);
        ASSERT_TRUE(server_->start());

        client_ = std::make_shared<prov::ProvClient>(socket_path_);
        adapter_ = std::make_unique<RealProvAdapter>(client_);
    }

    void TearDown() override {
        adapter_.reset();
        client_.reset();
        server_.reset();
        ::unlink(socket_path_.c_str());
    }

    std::string socket_path_;
    std::unique_ptr<StubProvServer> server_;
    std::shared_ptr<prov::ProvClient> client_;
    std::unique_ptr<RealProvAdapter> adapter_;
};

TEST_F(RealProvAdapterReconnectTest, AvailableAfterInitialConnect) {
    ASSERT_TRUE(client_->connect());
    EXPECT_TRUE(adapter_->is_available());
}

// 核心回归：连接被标记为断开后，is_available() 必须能重连。
// 修复前会永久返回 false，所有 PROV 调用恒返回 PROV_IPC_DISCONNECTED。
TEST_F(RealProvAdapterReconnectTest, ReconnectsAfterConnectionDropped) {
    ASSERT_TRUE(client_->connect());
    ASSERT_TRUE(adapter_->is_available());

    client_->disconnect();
    ASSERT_FALSE(client_->is_connected()) << "前置条件：连接应处于断开状态";

    EXPECT_TRUE(adapter_->is_available())
        << "PROV IPC 断连后未重连：write_vin / read_vin 将永久不可用";
    EXPECT_TRUE(client_->is_connected());
}

TEST_F(RealProvAdapterReconnectTest, ReconnectsRepeatedly) {
    ASSERT_TRUE(client_->connect());

    for (int i = 0; i < 3; ++i) {
        client_->disconnect();
        ASSERT_FALSE(client_->is_connected()) << "iteration " << i;
        std::this_thread::sleep_for(RealProvAdapter::kReconnectMinInterval +
                                    std::chrono::milliseconds(50));
        EXPECT_TRUE(adapter_->is_available()) << "iteration " << i;
    }
}

// 显式 reconnect() 不受最小间隔节流限制：工位/诊断流程需要能立即强制重连。
TEST_F(RealProvAdapterReconnectTest, ExplicitReconnectIsNotThrottled) {
    ASSERT_TRUE(client_->connect());
    client_->disconnect();

    // 先触发一次隐式重连尝试，使节流计时器开始计时。
    (void)adapter_->is_available();
    client_->disconnect();

    EXPECT_TRUE(adapter_->reconnect()) << "显式 reconnect() 不应被节流拦住";
    EXPECT_TRUE(client_->is_connected());
}

TEST_F(RealProvAdapterReconnectTest, UnavailableWhenServerIsGone) {
    ASSERT_TRUE(client_->connect());
    ASSERT_TRUE(adapter_->is_available());

    server_->stop();
    client_->disconnect();

    EXPECT_FALSE(adapter_->is_available());
}

TEST_F(RealProvAdapterReconnectTest, ThrottlesReconnectAttemptsWhileServerIsDown) {
    server_->stop();
    client_->disconnect();

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; ++i) {
        EXPECT_FALSE(adapter_->is_available());
    }
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, RealProvAdapter::kReconnectMinInterval * 10)
        << "重连未被节流，存在重连风暴风险";
}

TEST(RealProvAdapterTest, UnavailableWithNullClient) {
    RealProvAdapter adapter(nullptr);
    EXPECT_FALSE(adapter.is_available());
    EXPECT_FALSE(adapter.reconnect());
}

}  // namespace testing
}  // namespace diag
}  // namespace tbox
