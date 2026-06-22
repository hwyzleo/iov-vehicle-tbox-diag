#include <gtest/gtest.h>
#include "service_dispatcher.h"
#include "session_manager.h"
#include "security_access.h"
#include "mock_sec_interface.h"
#include "mock_prov_interface.h"
#include "constants.h"

namespace tbox {
namespace diag {
namespace testing {

class ServiceDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_sec = std::make_shared<MockSecInterface>();
        mock_prov = std::make_shared<MockProvInterface>();
        session_mgr = std::make_shared<SessionManager>();
        security_access = std::make_shared<SecurityAccess>(mock_sec);

        dispatcher = std::make_unique<ServiceDispatcher>(mock_prov, session_mgr, security_access);

        // Register default routes
        dispatcher->register_route(UdsService::ROUTINE_CONTROL, Rid::WRITE_VIN_ROUTINE, "PROV", true);
        dispatcher->register_route(UdsService::READ_DATA_BY_IDENTIFIER, Did::VIN, "PROV", false);
    }

    std::shared_ptr<MockSecInterface> mock_sec;
    std::shared_ptr<MockProvInterface> mock_prov;
    std::shared_ptr<SessionManager> session_mgr;
    std::shared_ptr<SecurityAccess> security_access;
    std::unique_ptr<ServiceDispatcher> dispatcher;
};

TEST_F(ServiceDispatcherTest, DiagnosticSessionControl) {
    DiagRequest request;
    request.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
    request.sub_function = UdsSession::EXTENDED;
    request.source_address = 0x0010;
    request.transport = TransportType::DOIP;

    auto response = dispatcher->dispatch(request);
    EXPECT_TRUE(response.positive);
    EXPECT_EQ(response.service_id, UdsService::DIAGNOSTIC_SESSION_CONTROL + 0x40);
    // payload = [P2_hi] [P2_lo] [P2*_hi] [P2*_lo], sub_function=session_type added by encoder
    ASSERT_EQ(response.payload.size(), 4u);
    // P2_DEFAULT = 5000ms = 0x1388
    EXPECT_EQ(response.payload[0], 0x13);
    EXPECT_EQ(response.payload[1], 0x88);
    // P2*_STAR = 5000ms / 10 = 500 = 0x01F4
    EXPECT_EQ(response.payload[2], 0x01);
    EXPECT_EQ(response.payload[3], 0xF4);
}

TEST_F(ServiceDispatcherTest, TesterPresent) {
    // First enter extended session
    DiagRequest session_req;
    session_req.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
    session_req.sub_function = UdsSession::EXTENDED;
    session_req.source_address = 0x0010;
    session_req.transport = TransportType::DOIP;
    dispatcher->dispatch(session_req);

    // Then TesterPresent
    DiagRequest request;
    request.service_id = UdsService::TESTER_PRESENT;
    request.sub_function = 0x00;
    request.source_address = 0x0010;

    auto response = dispatcher->dispatch(request);
    EXPECT_TRUE(response.positive);
    EXPECT_EQ(response.service_id, UdsService::TESTER_PRESENT + 0x40);
}

TEST_F(ServiceDispatcherTest, SecurityAccessRequestSeed) {
    DiagRequest request;
    request.service_id = UdsService::SECURITY_ACCESS;
    request.sub_function = UdsSecurityLevel::LEVEL_27;  // Request seed
    request.source_address = 0x0010;

    auto response = dispatcher->dispatch(request);
    EXPECT_TRUE(response.positive);
    EXPECT_FALSE(response.payload.empty());
}

TEST_F(ServiceDispatcherTest, SecurityAccessSendKey) {
    // First request seed
    DiagRequest seed_req;
    seed_req.service_id = UdsService::SECURITY_ACCESS;
    seed_req.sub_function = UdsSecurityLevel::LEVEL_27;
    seed_req.source_address = 0x0010;
    dispatcher->dispatch(seed_req);

    // Then send key
    DiagRequest key_req;
    key_req.service_id = UdsService::SECURITY_ACCESS;
    key_req.sub_function = (UdsSecurityLevel::LEVEL_27 + 1) | 0x80;
    key_req.payload = {0xAA, 0xBB, 0xCC, 0xDD};
    key_req.source_address = 0x0010;

    auto response = dispatcher->dispatch(key_req);
    EXPECT_TRUE(response.positive);
}

TEST_F(ServiceDispatcherTest, RoutineControlWithoutSecurity) {
    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;  // Start routine
    request.did_or_rid = Rid::WRITE_VIN_ROUTINE;
    request.payload = {'1', 'H', 'G', 'B', 'H', '4', '1', 'J', 'X', 'M', 'N', '1', '0', '9', '1', '8', '6'};
    request.source_address = 0x0010;

    auto response = dispatcher->dispatch(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::SECURITY_ACCESS_DENIED);
}

TEST_F(ServiceDispatcherTest, RoutineControlWithSecurity) {
    // Unlock security
    DiagRequest seed_req;
    seed_req.service_id = UdsService::SECURITY_ACCESS;
    seed_req.sub_function = UdsSecurityLevel::LEVEL_27;
    seed_req.source_address = 0x0010;
    dispatcher->dispatch(seed_req);

    DiagRequest key_req;
    key_req.service_id = UdsService::SECURITY_ACCESS;
    key_req.sub_function = (UdsSecurityLevel::LEVEL_27 + 1) | 0x80;
    key_req.payload = {0xAA};
    key_req.source_address = 0x0010;
    dispatcher->dispatch(key_req);

    // Now send VIN write
    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::WRITE_VIN_ROUTINE;
    request.payload = {'1', 'H', 'G', 'B', 'H', '4', '1', 'J', 'X', 'M', 'N', '1', '0', '9', '1', '8', '6'};
    request.source_address = 0x0010;

    auto response = dispatcher->dispatch(request);
    EXPECT_TRUE(response.positive);
    EXPECT_EQ(mock_prov->get_write_count(), 1);
}

TEST_F(ServiceDispatcherTest, RoutineControlInvalidPayload) {
    // Unlock security first
    DiagRequest seed_req;
    seed_req.service_id = UdsService::SECURITY_ACCESS;
    seed_req.sub_function = UdsSecurityLevel::LEVEL_27;
    seed_req.source_address = 0x0010;
    dispatcher->dispatch(seed_req);

    DiagRequest key_req;
    key_req.service_id = UdsService::SECURITY_ACCESS;
    key_req.sub_function = (UdsSecurityLevel::LEVEL_27 + 1) | 0x80;
    key_req.payload = {0xAA};
    key_req.source_address = 0x0010;
    dispatcher->dispatch(key_req);

    // Send with too-short payload
    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::WRITE_VIN_ROUTINE;
    request.payload = {0x01, 0x02};  // Too short
    request.source_address = 0x0010;

    auto response = dispatcher->dispatch(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::INCORRECT_MESSAGE_LENGTH);
}

TEST_F(ServiceDispatcherTest, ReadVinByIdentifier) {
    mock_prov->set_stored_vin("1HGBH41JXMN109186");

    DiagRequest request;
    request.service_id = UdsService::READ_DATA_BY_IDENTIFIER;
    request.did_or_rid = Did::VIN;
    request.source_address = 0x0010;

    auto response = dispatcher->dispatch(request);
    EXPECT_TRUE(response.positive);
    // Check VIN is in the response payload
    EXPECT_GE(response.payload.size(), 19);  // 2 bytes DID + 17 bytes VIN
}

TEST_F(ServiceDispatcherTest, UnsupportedService) {
    DiagRequest request;
    request.service_id = 0xFF;
    request.source_address = 0x0010;

    auto response = dispatcher->dispatch(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::SERVICE_NOT_SUPPORTED);
}

TEST_F(ServiceDispatcherTest, ProvUnavailable) {
    mock_prov->set_available(false);

    DiagRequest request;
    request.service_id = UdsService::READ_DATA_BY_IDENTIFIER;
    request.did_or_rid = Did::VIN;
    request.source_address = 0x0010;

    auto response = dispatcher->dispatch(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::CONDITIONS_NOT_CORRECT);
}

} // namespace testing
} // namespace diag
} // namespace tbox
