#include <gtest/gtest.h>
#include "doip_adapter.h"
#include "do_can_adapter.h"
#include "constants.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <future>

namespace tbox {
namespace diag {
namespace testing {

TEST(DoIpAdapterTest, CreateAdapter) {
    DoIpConfig config;
    config.listen_address = "127.0.0.1";
    config.port = 13400;

    DoIpAdapter adapter(config);
    EXPECT_EQ(adapter.get_transport_type(), TransportType::DOIP);
    EXPECT_FALSE(adapter.is_connected());
    EXPECT_TRUE(adapter.get_name().find("DoIP") != std::string::npos);
}

TEST(DoIpAdapterTest, StartServer) {
    DoIpConfig config;
    config.listen_address = "127.0.0.1";
    config.port = 0;  // Let OS pick port

    DoIpAdapter adapter(config);
    EXPECT_TRUE(adapter.start_server());
}

TEST(DoIpAdapterTest, ConnectWithoutServer) {
    DoIpConfig config;
    config.listen_address = "127.0.0.1";
    config.port = 13401;

    DoIpAdapter adapter(config);
    EXPECT_FALSE(adapter.connect());
    EXPECT_FALSE(adapter.is_connected());
}

TEST(DoIpAdapterTest, DisconnectWhenNotConnected) {
    DoIpConfig config;
    DoIpAdapter adapter(config);

    adapter.disconnect();
    EXPECT_FALSE(adapter.is_connected());
}

TEST(DoIpAdapterTest, SendWhenDisconnected) {
    DoIpConfig config;
    DoIpAdapter adapter(config);

    std::vector<uint8_t> data = {0x10, 0x03};
    EXPECT_FALSE(adapter.send(data));
}

TEST(DoIpAdapterTest, ReceiveWhenDisconnected) {
    DoIpConfig config;
    DoIpAdapter adapter(config);

    auto data = adapter.receive(100);
    EXPECT_TRUE(data.empty());
}

// 回归测试: 握手失败后 connect() 曾经在持有 mutex_ 的情况下调用 disconnect()，
// 导致 std::mutex 自锁死锁，主循环永久卡住、不再 accept 任何 DoIP 连接。
// 该测试在修复前会在 connect() 处永久挂起。
TEST(DoIpAdapterTest, HandshakeFailureDoesNotDeadlock) {
    DoIpConfig config;
    config.listen_address = "127.0.0.1";
    config.port = 13502;
    config.accept_timeout_ms = 2000;

    DoIpAdapter adapter(config);
    ASSERT_TRUE(adapter.start_server());

    // 建立一个不发送路由激活请求的客户端连接，强制握手超时(3s)。
    int client = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client, 0);
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.port);
    ASSERT_EQ(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr), 1);
    ASSERT_EQ(::connect(client, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)), 0);

    auto fut = std::async(std::launch::async, [&adapter]() { return adapter.connect(); });
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(10)), std::future_status::ready)
        << "connect() 未返回: 握手失败路径发生死锁";
    EXPECT_FALSE(fut.get());

    // 死锁修复后，适配器仍可继续使用（这些调用同样需要 mutex_）。
    auto fut2 = std::async(std::launch::async, [&adapter]() { return adapter.is_connected(); });
    ASSERT_EQ(fut2.wait_for(std::chrono::seconds(5)), std::future_status::ready)
        << "is_connected() 阻塞: mutex_ 仍被持有";
    EXPECT_FALSE(fut2.get());

    ::close(client);
}

TEST(DoCanAdapterTest, CreateAdapter) {
    DoCanConfig config;
    config.can_interface = "can0";
    config.tx_id = 0x7E0;
    config.rx_id = 0x7E8;

    DoCanAdapter adapter(config);
    EXPECT_EQ(adapter.get_transport_type(), TransportType::DO_CAN);
    EXPECT_FALSE(adapter.is_connected());
    EXPECT_TRUE(adapter.get_name().find("DoCAN") != std::string::npos);
}

TEST(DoCanAdapterTest, Connect) {
    DoCanConfig config;
    DoCanAdapter adapter(config);

    EXPECT_TRUE(adapter.connect());
    EXPECT_TRUE(adapter.is_connected());
}

TEST(DoCanAdapterTest, Disconnect) {
    DoCanConfig config;
    DoCanAdapter adapter(config);

    adapter.connect();
    adapter.disconnect();
    EXPECT_FALSE(adapter.is_connected());
}

TEST(DoCanAdapterTest, SendWhenDisconnected) {
    DoCanConfig config;
    DoCanAdapter adapter(config);

    std::vector<uint8_t> data = {0x10, 0x03};
    EXPECT_FALSE(adapter.send(data));
}

} // namespace testing
} // namespace diag
} // namespace tbox
