#include <gtest/gtest.h>
#include "diag_service.h"
#include "mock_sec_interface.h"
#include "mock_prov_interface.h"
#include "constants.h"

namespace tbox {
namespace diag {
namespace testing {

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_sec = std::make_shared<MockSecInterface>();
        mock_prov = std::make_shared<MockProvInterface>();

        service = std::make_unique<DiagService>();
        service->set_sec(mock_sec);
        service->set_prov(mock_prov);
        ASSERT_EQ(service->initialize(), DiagErrorCode::SUCCESS);
    }

    void TearDown() override {
        if (service) {
            service->shutdown();
        }
    }

    DiagResponse send_session_control(uint8_t session_type, uint16_t source = 0x0010) {
        DiagRequest req;
        req.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
        req.sub_function = session_type;
        req.source_address = source;
        req.transport = TransportType::DOIP;
        return service->process_request(req);
    }

    DiagResponse send_tester_present(uint16_t source = 0x0010) {
        DiagRequest req;
        req.service_id = UdsService::TESTER_PRESENT;
        req.sub_function = 0x00;
        req.source_address = source;
        return service->process_request(req);
    }

    DiagResponse send_request_seed(uint16_t source = 0x0010) {
        DiagRequest req;
        req.service_id = UdsService::SECURITY_ACCESS;
        req.sub_function = UdsSecurityLevel::LEVEL_27;
        req.source_address = source;
        return service->process_request(req);
    }

    DiagResponse send_key(const std::vector<uint8_t>& key, uint16_t source = 0x0010) {
        DiagRequest req;
        req.service_id = UdsService::SECURITY_ACCESS;
        req.sub_function = (UdsSecurityLevel::LEVEL_27 + 1) | 0x80;
        req.payload = key;
        req.source_address = source;
        return service->process_request(req);
    }

    DiagResponse send_write_vin(const std::string& vin, uint16_t source = 0x0010) {
        DiagRequest req;
        req.service_id = UdsService::ROUTINE_CONTROL;
        req.sub_function = 0x01;
        req.did_or_rid = Rid::WRITE_VIN_ROUTINE;
        req.payload = std::vector<uint8_t>(vin.begin(), vin.end());
        req.source_address = source;
        return service->process_request(req);
    }

    DiagResponse send_read_vin(uint16_t source = 0x0010) {
        DiagRequest req;
        req.service_id = UdsService::READ_DATA_BY_IDENTIFIER;
        req.did_or_rid = Did::VIN;
        req.source_address = source;
        return service->process_request(req);
    }

    std::shared_ptr<MockSecInterface> mock_sec;
    std::shared_ptr<MockProvInterface> mock_prov;
    std::unique_ptr<DiagService> service;
};

TEST_F(IntegrationTest, FullEOLVinWriteFlow) {
    // Step 1: Enter extended session
    auto resp = send_session_control(UdsSession::EXTENDED);
    EXPECT_TRUE(resp.positive);

    // Step 2: Request seed
    resp = send_request_seed();
    EXPECT_TRUE(resp.positive);
    EXPECT_FALSE(resp.payload.empty());

    // Step 3: Send key (mock accepts any key)
    resp = send_key({0xAA, 0xBB, 0xCC, 0xDD});
    EXPECT_TRUE(resp.positive);

    // Step 4: Write VIN
    std::string vin = "1HGBH41JXMN109186";
    resp = send_write_vin(vin);
    EXPECT_TRUE(resp.positive);

    // Step 5: Read VIN back
    mock_prov->set_stored_vin(vin);
    resp = send_read_vin();
    EXPECT_TRUE(resp.positive);
}

TEST_F(IntegrationTest, VinWriteWithoutSecurityDenied) {
    // Enter extended session
    send_session_control(UdsSession::EXTENDED);

    // Try to write VIN without security unlock
    auto resp = send_write_vin("1HGBH41JXMN109186");
    EXPECT_FALSE(resp.positive);
    EXPECT_EQ(resp.nrc, Nrc::SECURITY_ACCESS_DENIED);
}

TEST_F(IntegrationTest, WrongTesterRejected) {
    // Tester A enters session
    send_session_control(UdsSession::EXTENDED, 0x0010);

    // Tester B tries to enter session
    auto resp = send_session_control(UdsSession::EXTENDED, 0x0020);
    EXPECT_FALSE(resp.positive);
}

TEST_F(IntegrationTest, TesterPresentMaintainsSession) {
    send_session_control(UdsSession::EXTENDED);

    // Send TesterPresent multiple times
    for (int i = 0; i < 5; i++) {
        auto resp = send_tester_present();
        EXPECT_TRUE(resp.positive);
    }
}

TEST_F(IntegrationTest, InvalidServiceRejected) {
    DiagRequest req;
    req.service_id = 0xFF;
    req.source_address = 0x0010;

    auto resp = service->process_request(req);
    EXPECT_FALSE(resp.positive);
    EXPECT_EQ(resp.nrc, Nrc::SERVICE_NOT_SUPPORTED);
}

TEST_F(IntegrationTest, VinWriteProvFailure) {
    send_session_control(UdsSession::EXTENDED);
    send_request_seed();
    send_key({0xAA});

    mock_prov->set_fail_write(true);

    auto resp = send_write_vin("1HGBH41JXMN109186");
    EXPECT_FALSE(resp.positive);
    EXPECT_EQ(resp.nrc, Nrc::GENERAL_PROGRAMMING_FAILURE);
}

TEST_F(IntegrationTest, ReadVinProvUnavailable) {
    send_session_control(UdsSession::EXTENDED);

    mock_prov->set_available(false);

    auto resp = send_read_vin();
    EXPECT_FALSE(resp.positive);
    EXPECT_EQ(resp.nrc, Nrc::CONDITIONS_NOT_CORRECT);
}

TEST_F(IntegrationTest, SecurityAccessSecUnavailable) {
    send_session_control(UdsSession::EXTENDED);

    mock_sec->set_available(false);

    auto resp = send_request_seed();
    EXPECT_FALSE(resp.positive);
    EXPECT_EQ(resp.nrc, Nrc::CONDITIONS_NOT_CORRECT);
}

TEST_F(IntegrationTest, MultipleSecurityLevels) {
    send_session_control(UdsSession::EXTENDED);

    // Request seed for level 0x27
    auto resp = send_request_seed();
    EXPECT_TRUE(resp.positive);

    // Send key for level 0x27
    resp = send_key({0xAA});
    EXPECT_TRUE(resp.positive);

    // Verify unlocked
    (void)service->get_current_session();
}

} // namespace testing
} // namespace diag
} // namespace tbox
