#include "diag_service.h"
#include "mock_sec_interface.h"
#include "mock_prov_interface.h"
#include <gtest/gtest.h>

using namespace tbox::diag;

class CertificateRequestTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_prov_ = std::make_shared<MockProvInterface>();
        mock_sec_ = std::make_shared<MockSecInterface>();
        diag_service_ = std::make_shared<DiagService>();
        diag_service_->set_prov(mock_prov_);
        diag_service_->set_sec(mock_sec_);
        diag_service_->initialize();
    }

    void TearDown() override {
        if (diag_service_) {
            diag_service_->shutdown();
        }
    }

    void switch_to_programming_session() {
        DiagRequest session_req;
        session_req.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
        session_req.sub_function = UdsSession::PROGRAMMING;
        session_req.source_address = 0x0010;
        session_req.transport = TransportType::DOIP;
        diag_service_->process_request(session_req);
    }

    void switch_to_extended_session() {
        DiagRequest session_req;
        session_req.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
        session_req.sub_function = UdsSession::EXTENDED;
        session_req.source_address = 0x0010;
        session_req.transport = TransportType::DOIP;
        diag_service_->process_request(session_req);
    }

    void unlock_security_access() {
        DiagRequest seed_request;
        seed_request.service_id = UdsService::SECURITY_ACCESS;
        seed_request.sub_function = UdsSecurityLevel::LEVEL_27;
        seed_request.source_address = 0x0010;
        diag_service_->process_request(seed_request);

        DiagRequest key_request;
        key_request.service_id = UdsService::SECURITY_ACCESS;
        key_request.sub_function = (UdsSecurityLevel::LEVEL_27 + 1) | 0x80;
        key_request.payload = {0x12, 0x34, 0x56, 0x78};
        key_request.source_address = 0x0010;
        diag_service_->process_request(key_request);
    }

    std::shared_ptr<MockProvInterface> mock_prov_;
    std::shared_ptr<MockSecInterface> mock_sec_;
    std::shared_ptr<DiagService> diag_service_;
};

TEST_F(CertificateRequestTest, SuccessfulCertificateRequest) {
    switch_to_programming_session();
    unlock_security_access();

    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::GENERATE_KEY_PAIR;
    request.payload = {};
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_TRUE(response.positive);
    EXPECT_EQ(response.service_id, UdsService::ROUTINE_CONTROL + 0x40);
    EXPECT_EQ(response.sub_function, 0x01);
}

TEST_F(CertificateRequestTest, CertificateRequestWithoutSecurity) {
    switch_to_programming_session();

    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::GENERATE_KEY_PAIR;
    request.payload = {};
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::SECURITY_ACCESS_DENIED);
}

TEST_F(CertificateRequestTest, CertificateRequestWithSecurity) {
    switch_to_programming_session();
    unlock_security_access();

    DiagRequest cert_request;
    cert_request.service_id = UdsService::ROUTINE_CONTROL;
    cert_request.sub_function = 0x01;
    cert_request.did_or_rid = Rid::GENERATE_KEY_PAIR;
    cert_request.payload = {};
    cert_request.source_address = 0x0010;

    auto cert_response = diag_service_->process_request(cert_request);
    EXPECT_TRUE(cert_response.positive);
}

// Test handle_read_csr() - 正向路径（CSR 返回）
TEST_F(CertificateRequestTest, ReadCsrSuccess) {
    switch_to_programming_session();
    unlock_security_access();

    std::vector<uint8_t> expected_csr = {0x30, 0x82, 0x02, 0x10, 0x30, 0x82};
    mock_sec_->set_csr_data(expected_csr);
    mock_sec_->set_get_csr_result(true);

    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::READ_CSR;
    request.payload = {};
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_TRUE(response.positive);
    EXPECT_EQ(response.service_id, UdsService::ROUTINE_CONTROL + 0x40);
    EXPECT_EQ(response.sub_function, 0x01);
    // Response should contain RID echo + CSR data
    ASSERT_GE(response.payload.size(), 2u + expected_csr.size());
    EXPECT_EQ(response.payload[0], (Rid::READ_CSR >> 8) & 0xFF);
    EXPECT_EQ(response.payload[1], Rid::READ_CSR & 0xFF);
    std::vector<uint8_t> csr_in_response(response.payload.begin() + 2, response.payload.end());
    EXPECT_EQ(csr_in_response, expected_csr);
}

// Test handle_read_csr() - SEC 不可用
TEST_F(CertificateRequestTest, ReadCsrSecUnavailable) {
    switch_to_programming_session();
    unlock_security_access();

    mock_sec_->set_available(false);

    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::READ_CSR;
    request.payload = {};
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::CONDITIONS_NOT_CORRECT);
}

// Test handle_read_csr() - CSR 创建失败
TEST_F(CertificateRequestTest, ReadCsrCreationFailed) {
    switch_to_programming_session();
    unlock_security_access();

    mock_sec_->set_get_csr_result(false);

    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::READ_CSR;
    request.payload = {};
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::GENERAL_PROGRAMMING_FAILURE);
}

// Test handle_inject_certificate() - 正向路径
TEST_F(CertificateRequestTest, InjectCertificateSuccess) {
    switch_to_programming_session();
    unlock_security_access();

    std::vector<uint8_t> cert_data = {0x30, 0x82, 0x03, 0x10, 0x30, 0x82, 0x02, 0x10};
    mock_sec_->set_inject_certificate_result(true);

    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::INJECT_CERTIFICATE;
    request.payload = cert_data;
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_TRUE(response.positive);
    EXPECT_EQ(response.service_id, UdsService::ROUTINE_CONTROL + 0x40);
    EXPECT_EQ(response.sub_function, 0x01);
    // Response should contain RID echo
    ASSERT_EQ(response.payload.size(), 2u);
    EXPECT_EQ(response.payload[0], (Rid::INJECT_CERTIFICATE >> 8) & 0xFF);
    EXPECT_EQ(response.payload[1], Rid::INJECT_CERTIFICATE & 0xFF);
    // Verify certificate was injected
    EXPECT_EQ(mock_sec_->get_injected_certificate(), cert_data);
}

// Test handle_inject_certificate() - 空载荷拒绝
TEST_F(CertificateRequestTest, InjectCertificateEmptyPayload) {
    switch_to_programming_session();
    unlock_security_access();

    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::INJECT_CERTIFICATE;
    request.payload = {};
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::INCORRECT_MESSAGE_LENGTH);
}

// Test handle_inject_certificate() - 注入失败
TEST_F(CertificateRequestTest, InjectCertificateFailed) {
    switch_to_programming_session();
    unlock_security_access();

    mock_sec_->set_inject_certificate_result(false);

    std::vector<uint8_t> cert_data = {0x30, 0x82, 0x03, 0x10};
    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::INJECT_CERTIFICATE;
    request.payload = cert_data;
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::GENERAL_PROGRAMMING_FAILURE);
}

// Test session type enforcement - 在扩展会话中请求证书 RID 应返回 NRC 0x7F
TEST_F(CertificateRequestTest, CertificateRidInExtendedSessionReturnsNrc7F) {
    switch_to_extended_session();
    unlock_security_access();

    // Test GENERATE_KEY_PAIR
    DiagRequest request1;
    request1.service_id = UdsService::ROUTINE_CONTROL;
    request1.sub_function = 0x01;
    request1.did_or_rid = Rid::GENERATE_KEY_PAIR;
    request1.payload = {};
    request1.source_address = 0x0010;

    auto response1 = diag_service_->process_request(request1);
    EXPECT_FALSE(response1.positive);
    EXPECT_EQ(response1.nrc, Nrc::SERVICE_NOT_SUPPORTED_IN_SESSION);

    // Test READ_CSR
    DiagRequest request2;
    request2.service_id = UdsService::ROUTINE_CONTROL;
    request2.sub_function = 0x01;
    request2.did_or_rid = Rid::READ_CSR;
    request2.payload = {};
    request2.source_address = 0x0010;

    auto response2 = diag_service_->process_request(request2);
    EXPECT_FALSE(response2.positive);
    EXPECT_EQ(response2.nrc, Nrc::SERVICE_NOT_SUPPORTED_IN_SESSION);

    // Test INJECT_CERTIFICATE
    DiagRequest request3;
    request3.service_id = UdsService::ROUTINE_CONTROL;
    request3.sub_function = 0x01;
    request3.did_or_rid = Rid::INJECT_CERTIFICATE;
    request3.payload = {0x30, 0x82};
    request3.source_address = 0x0010;

    auto response3 = diag_service_->process_request(request3);
    EXPECT_FALSE(response3.positive);
    EXPECT_EQ(response3.nrc, Nrc::SERVICE_NOT_SUPPORTED_IN_SESSION);
}
