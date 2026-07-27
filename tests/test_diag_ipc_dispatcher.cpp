#include <gtest/gtest.h>
#include "diag_ipc_dispatcher.h"
#include "diag_service.h"
#include "diag_ipc_protocol.h"
#include "data_models.h"
#include "error_codes.h"
#include <nlohmann/json.hpp>

using namespace tbox::diag;
using json = nlohmann::json;

// ============================================================
// MockDiagService - overrides virtual methods for testing
// ============================================================
class MockDiagService : public DiagService {
public:
    MockDiagService() : DiagService() {}

    // Control flags / test data
    bool mock_initialized = true;
    DiagSession mock_session{};
    bool mock_tester_connected = false;
    VinReadResult mock_vehicle_info{};

    // Overrides
    bool is_initialized() const override {
        return mock_initialized;
    }

    DiagSession get_current_session() const override {
        return mock_session;
    }

    bool is_tester_connected() const override {
        return mock_tester_connected;
    }

    VinReadResult get_vehicle_info() override {
        return mock_vehicle_info;
    }
};

// ============================================================
// Tests
// ============================================================

class DiagIpcDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_service = std::make_unique<MockDiagService>();
        dispatcher = std::make_unique<DiagIpcDispatcher>(mock_service.get());
    }

    std::unique_ptr<MockDiagService> mock_service;
    std::unique_ptr<DiagIpcDispatcher> dispatcher;
};

TEST_F(DiagIpcDispatcherTest, GetServiceStatus) {
    mock_service->mock_initialized = true;
    mock_service->mock_session.session_type = SessionType::EXTENDED;
    mock_service->mock_session.state = SessionState::SECURITY_UNLOCKED;
    mock_service->mock_session.security_unlocked = true;
    mock_service->mock_session.transport = TransportType::DOIP;

    std::string response = dispatcher->dispatch(
        static_cast<uint32_t>(ipc::MethodId::GET_SERVICE_STATUS), "{}", 0);

    auto j = json::parse(response);
    EXPECT_EQ(j["status"], static_cast<int>(DiagErrorCode::SUCCESS));
    EXPECT_TRUE(j["initialized"].get<bool>());
    EXPECT_EQ(j["session_type"], "EXTENDED");
    EXPECT_EQ(j["session_state"], "SECURITY_UNLOCKED");
    EXPECT_TRUE(j["security_unlocked"].get<bool>());
    EXPECT_EQ(j["transport"], "DOIP");
}

TEST_F(DiagIpcDispatcherTest, GetServiceStatusNotInitialized) {
    mock_service->mock_initialized = false;
    mock_service->mock_session.state = SessionState::IDLE;

    std::string response = dispatcher->dispatch(
        static_cast<uint32_t>(ipc::MethodId::GET_SERVICE_STATUS), "{}", 0);

    auto j = json::parse(response);
    EXPECT_EQ(j["status"], static_cast<int>(DiagErrorCode::SUCCESS));
    EXPECT_FALSE(j["initialized"].get<bool>());
    EXPECT_EQ(j["session_state"], "IDLE");
}

TEST_F(DiagIpcDispatcherTest, GetVehicleInfo) {
    mock_service->mock_vehicle_info.valid = true;
    mock_service->mock_vehicle_info.vin = "LSJU26H92PS000017";
    mock_service->mock_vehicle_info.bind_state = "BOUND";

    std::string response = dispatcher->dispatch(
        static_cast<uint32_t>(ipc::MethodId::GET_VEHICLE_INFO), "{}", 0);

    auto j = json::parse(response);
    EXPECT_EQ(j["status"], static_cast<int>(DiagErrorCode::SUCCESS));
    EXPECT_TRUE(j["valid"].get<bool>());
    EXPECT_EQ(j["vin"], "LSJU26H92PS000017");
    EXPECT_EQ(j["bind_state"], "BOUND");
}

TEST_F(DiagIpcDispatcherTest, GetVehicleInfoInvalid) {
    mock_service->mock_vehicle_info.valid = false;

    std::string response = dispatcher->dispatch(
        static_cast<uint32_t>(ipc::MethodId::GET_VEHICLE_INFO), "{}", 0);

    auto j = json::parse(response);
    EXPECT_EQ(j["status"], static_cast<int>(DiagErrorCode::SUCCESS));
    EXPECT_FALSE(j["valid"].get<bool>());
    EXPECT_FALSE(j.contains("vin"));
}

TEST_F(DiagIpcDispatcherTest, IsTesterConnected) {
    mock_service->mock_tester_connected = true;

    std::string response = dispatcher->dispatch(
        static_cast<uint32_t>(ipc::MethodId::IS_TESTER_CONNECTED), "{}", 0);

    auto j = json::parse(response);
    EXPECT_EQ(j["status"], static_cast<int>(DiagErrorCode::SUCCESS));
    EXPECT_TRUE(j["connected"].get<bool>());
}

TEST_F(DiagIpcDispatcherTest, IsTesterNotConnected) {
    mock_service->mock_tester_connected = false;

    std::string response = dispatcher->dispatch(
        static_cast<uint32_t>(ipc::MethodId::IS_TESTER_CONNECTED), "{}", 0);

    auto j = json::parse(response);
    EXPECT_EQ(j["status"], static_cast<int>(DiagErrorCode::SUCCESS));
    EXPECT_FALSE(j["connected"].get<bool>());
}

TEST_F(DiagIpcDispatcherTest, UnknownMethod) {
    std::string response = dispatcher->dispatch(9999, "{}", 0);

    auto j = json::parse(response);
    // FW-0306: unknown method
    EXPECT_EQ(j["status"], 306);
    EXPECT_TRUE(j.contains("error"));
}

TEST_F(DiagIpcDispatcherTest, AllMethodsReturnStatusField) {
    // Verify that every response contains a "status" field
    for (uint32_t method = 1; method <= 3; ++method) {
        std::string response = dispatcher->dispatch(method, "{}", 0);
        auto j = json::parse(response);
        EXPECT_TRUE(j.contains("status")) << "Method " << method << " missing status field";
    }
}
