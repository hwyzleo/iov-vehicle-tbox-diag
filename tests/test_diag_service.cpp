#include <gtest/gtest.h>
#include "diag_service.h"
#include "mock_sec_interface.h"
#include "mock_prov_interface.h"
#include "constants.h"

namespace tbox {
namespace diag {
namespace testing {

class DiagServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_sec = std::make_shared<MockSecInterface>();
        mock_prov = std::make_shared<MockProvInterface>();

        service = std::make_unique<DiagService>();
        service->set_sec(mock_sec);
        service->set_prov(mock_prov);
    }

    void TearDown() override {
        if (service) {
            service->shutdown();
        }
    }

    std::shared_ptr<MockSecInterface> mock_sec;
    std::shared_ptr<MockProvInterface> mock_prov;
    std::unique_ptr<DiagService> service;
};

TEST_F(DiagServiceTest, Initialize) {
    EXPECT_EQ(service->initialize(), DiagErrorCode::SUCCESS);
    EXPECT_TRUE(service->is_initialized());
}

TEST_F(DiagServiceTest, DoubleInitialize) {
    EXPECT_EQ(service->initialize(), DiagErrorCode::SUCCESS);
    EXPECT_EQ(service->initialize(), DiagErrorCode::SUCCESS);
}

TEST_F(DiagServiceTest, InitializeWithoutSec) {
    DiagService svc;
    svc.set_prov(mock_prov);
    EXPECT_EQ(svc.initialize(), DiagErrorCode::SEC_UNAVAILABLE);
}

TEST_F(DiagServiceTest, InitializeWithoutProv) {
    DiagService svc;
    svc.set_sec(mock_sec);
    EXPECT_EQ(svc.initialize(), DiagErrorCode::PROV_UNAVAILABLE);
}

TEST_F(DiagServiceTest, ProcessRequestBeforeInit) {
    DiagRequest request;
    request.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
    request.sub_function = UdsSession::EXTENDED;

    auto response = service->process_request(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::CONDITIONS_NOT_CORRECT);
}

TEST_F(DiagServiceTest, FullDiagnosticFlow) {
    ASSERT_EQ(service->initialize(), DiagErrorCode::SUCCESS);

    // 1. Switch to extended session
    DiagRequest session_req;
    session_req.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
    session_req.sub_function = UdsSession::EXTENDED;
    session_req.source_address = 0x0010;
    session_req.transport = TransportType::DOIP;

    auto resp = service->process_request(session_req);
    EXPECT_TRUE(resp.positive);

    // 2. Request seed
    DiagRequest seed_req;
    seed_req.service_id = UdsService::SECURITY_ACCESS;
    seed_req.sub_function = UdsSecurityLevel::LEVEL_27;
    seed_req.source_address = 0x0010;

    resp = service->process_request(seed_req);
    EXPECT_TRUE(resp.positive);
    EXPECT_FALSE(resp.payload.empty());

    // 3. Send key
    DiagRequest key_req;
    key_req.service_id = UdsService::SECURITY_ACCESS;
    key_req.sub_function = (UdsSecurityLevel::LEVEL_27 + 1) | 0x80;
    key_req.payload = {0xAA, 0xBB, 0xCC, 0xDD};
    key_req.source_address = 0x0010;

    resp = service->process_request(key_req);
    EXPECT_TRUE(resp.positive);

    // 4. Write VIN via RoutineControl
    DiagRequest write_req;
    write_req.service_id = UdsService::ROUTINE_CONTROL;
    write_req.sub_function = 0x01;
    write_req.did_or_rid = Rid::WRITE_VIN_ROUTINE;
    write_req.payload = {'1', 'H', 'G', 'B', 'H', '4', '1', 'J', 'X', 'M', 'N', '1', '0', '9', '1', '8', '6'};
    write_req.source_address = 0x0010;

    resp = service->process_request(write_req);
    EXPECT_TRUE(resp.positive);

    // 5. Read VIN back
    mock_prov->set_stored_vin("1HGBH41JXMN109186");

    DiagRequest read_req;
    read_req.service_id = UdsService::READ_DATA_BY_IDENTIFIER;
    read_req.did_or_rid = Did::VIN;
    read_req.source_address = 0x0010;

    resp = service->process_request(read_req);
    EXPECT_TRUE(resp.positive);
}

TEST_F(DiagServiceTest, Shutdown) {
    ASSERT_EQ(service->initialize(), DiagErrorCode::SUCCESS);

    service->shutdown();
    EXPECT_FALSE(service->is_initialized());
}

TEST_F(DiagServiceTest, GetCurrentSession) {
    ASSERT_EQ(service->initialize(), DiagErrorCode::SUCCESS);

    auto session = service->get_current_session();
    EXPECT_EQ(session.session_type, SessionType::DEFAULT);
}

} // namespace testing
} // namespace diag
} // namespace tbox
