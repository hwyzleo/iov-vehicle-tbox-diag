#include <gtest/gtest.h>
#include "doip_adapter.h"
#include "do_can_adapter.h"
#include "constants.h"

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
