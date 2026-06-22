#include "diag_service.h"
#include "stub_sec.h"
#include "stub_prov.h"
#include <gtest/gtest.h>

using namespace tbox::diag;

class CertificateRequestTest : public ::testing::Test {
protected:
    void SetUp() override {
        prov_ = std::make_shared<StubProvInterface>();
        sec_ = std::make_shared<StubSecInterface>();
        diag_service_ = std::make_shared<DiagService>();
        diag_service_->set_prov(prov_);
        diag_service_->set_sec(sec_);
        diag_service_->initialize();
    }

    void TearDown() override {
        if (diag_service_) {
            diag_service_->shutdown();
        }
    }

    std::shared_ptr<StubProvInterface> prov_;
    std::shared_ptr<StubSecInterface> sec_;
    std::shared_ptr<DiagService> diag_service_;
};

TEST_F(CertificateRequestTest, SuccessfulCertificateRequest) {
    // 先切换到扩展会话
    DiagRequest session_req;
    session_req.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
    session_req.sub_function = UdsSession::EXTENDED;
    session_req.source_address = 0x0010;
    session_req.transport = TransportType::DOIP;
    diag_service_->process_request(session_req);

    // 解锁安全访问
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

    // 发送证书申请请求
    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::CERTIFICATE_REQUEST;
    request.payload = {};
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_TRUE(response.positive);
    EXPECT_EQ(response.service_id, UdsService::ROUTINE_CONTROL + 0x40);
    EXPECT_EQ(response.sub_function, 0x01);
}

TEST_F(CertificateRequestTest, CertificateRequestWithoutSecurity) {
    // 先切换到扩展会话
    DiagRequest session_req;
    session_req.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
    session_req.sub_function = UdsSession::EXTENDED;
    session_req.source_address = 0x0010;
    session_req.transport = TransportType::DOIP;
    diag_service_->process_request(session_req);

    // 不解锁安全访问，直接发送证书申请请求
    DiagRequest request;
    request.service_id = UdsService::ROUTINE_CONTROL;
    request.sub_function = 0x01;
    request.did_or_rid = Rid::CERTIFICATE_REQUEST;
    request.payload = {};
    request.source_address = 0x0010;

    auto response = diag_service_->process_request(request);
    EXPECT_FALSE(response.positive);
    EXPECT_EQ(response.nrc, Nrc::SECURITY_ACCESS_DENIED);
}

TEST_F(CertificateRequestTest, CertificateRequestWithSecurity) {
    // 先切换到扩展会话
    DiagRequest session_req;
    session_req.service_id = UdsService::DIAGNOSTIC_SESSION_CONTROL;
    session_req.sub_function = UdsSession::EXTENDED;
    session_req.source_address = 0x0010;
    session_req.transport = TransportType::DOIP;
    diag_service_->process_request(session_req);

    // 请求种子
    DiagRequest seed_request;
    seed_request.service_id = UdsService::SECURITY_ACCESS;
    seed_request.sub_function = UdsSecurityLevel::LEVEL_27;
    seed_request.source_address = 0x0010;

    auto seed_response = diag_service_->process_request(seed_request);
    EXPECT_TRUE(seed_response.positive);

    // 发送密钥
    DiagRequest key_request;
    key_request.service_id = UdsService::SECURITY_ACCESS;
    key_request.sub_function = (UdsSecurityLevel::LEVEL_27 + 1) | 0x80;
    key_request.payload = {0x12, 0x34, 0x56, 0x78};
    key_request.source_address = 0x0010;

    auto key_response = diag_service_->process_request(key_request);
    EXPECT_TRUE(key_response.positive);

    // 现在可以发送证书申请请求
    DiagRequest cert_request;
    cert_request.service_id = UdsService::ROUTINE_CONTROL;
    cert_request.sub_function = 0x01;
    cert_request.did_or_rid = Rid::CERTIFICATE_REQUEST;
    cert_request.payload = {};
    cert_request.source_address = 0x0010;

    auto cert_response = diag_service_->process_request(cert_request);
    EXPECT_TRUE(cert_response.positive);
}
