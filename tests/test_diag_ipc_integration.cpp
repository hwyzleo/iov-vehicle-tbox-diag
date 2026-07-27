#include <gtest/gtest.h>
#include "diag_service.h"
#include "diag_ipc_protocol.h"
#include "tbox/diag/client.h"
#include "tbox/diag/types.h"
#include "tbox/diag/errors.h"
#include "mock_sec_interface.h"
#include "mock_prov_interface.h"
#include "config.h"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <unistd.h>

using namespace tbox::diag;
using namespace std::chrono_literals;

// ============================================================
// Integration test: framework-ipc Server + Client loopback
// ============================================================

class DiagIpcIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate unique socket path per test to avoid conflicts
        socket_path_ = "/tmp/tbox-diag-test-" +
                       std::to_string(getpid()) + "-" +
                       std::to_string(rand()) + ".sock";

        mock_sec = std::make_shared<MockSecInterface>();
        mock_prov = std::make_shared<MockProvInterface>();

        DiagServiceConfig config;
        config.ipc_socket_path = socket_path_;
        service = std::make_unique<DiagService>(config);
        service->set_sec(mock_sec);
        service->set_prov(mock_prov);

        ASSERT_EQ(service->initialize(), DiagErrorCode::SUCCESS);
        ASSERT_TRUE(service->start_ipc_server());

        // Give the server a moment to bind
        std::this_thread::sleep_for(50ms);
    }

    void TearDown() override {
        if (service) {
            service->stop_ipc_server();
            service->shutdown();
        }
        // Clean up socket file
        unlink(socket_path_.c_str());
    }

    std::string socket_path_;
    std::shared_ptr<MockSecInterface> mock_sec;
    std::shared_ptr<MockProvInterface> mock_prov;
    std::unique_ptr<DiagService> service;
};

TEST_F(DiagIpcIntegrationTest, GetServiceStatus) {
    DiagClient client(socket_path_);
    ASSERT_TRUE(client.connect());

    DiagServiceStatus status;
    auto err = client.get_service_status(status);
    EXPECT_EQ(err, DiagClientError::SUCCESS);
    EXPECT_TRUE(status.initialized);
    EXPECT_FALSE(status.session_type.empty());
}

TEST_F(DiagIpcIntegrationTest, IsTesterConnected) {
    DiagClient client(socket_path_);
    ASSERT_TRUE(client.connect());

    bool connected = true;
    auto err = client.is_tester_connected(connected);
    EXPECT_EQ(err, DiagClientError::SUCCESS);
    // No DTE connected in test
    EXPECT_FALSE(connected);
}

TEST_F(DiagIpcIntegrationTest, GetVehicleInfo) {
    DiagClient client(socket_path_);
    ASSERT_TRUE(client.connect());

    VehicleInfo info;
    auto err = client.get_vehicle_info(info);
    EXPECT_EQ(err, DiagClientError::SUCCESS);
    // MockProvInterface returns empty VIN, so valid may be false
}

TEST_F(DiagIpcIntegrationTest, MultipleCalls) {
    DiagClient client(socket_path_);
    ASSERT_TRUE(client.connect());

    // Multiple sequential calls on same connection
    for (int i = 0; i < 5; ++i) {
        DiagServiceStatus status;
        auto err = client.get_service_status(status);
        EXPECT_EQ(err, DiagClientError::SUCCESS);
    }
}

TEST_F(DiagIpcIntegrationTest, DisconnectReconnect) {
    DiagClient client(socket_path_);
    ASSERT_TRUE(client.connect());
    client.disconnect();
    EXPECT_FALSE(client.is_connected());

    ASSERT_TRUE(client.connect());
    EXPECT_TRUE(client.is_connected());

    DiagServiceStatus status;
    auto err = client.get_service_status(status);
    EXPECT_EQ(err, DiagClientError::SUCCESS);
}

TEST_F(DiagIpcIntegrationTest, NotConnectedReturnsError) {
    DiagClient client(socket_path_);
    // Don't connect
    DiagServiceStatus status;
    auto err = client.get_service_status(status);
    EXPECT_EQ(err, DiagClientError::NOT_CONNECTED);
}
