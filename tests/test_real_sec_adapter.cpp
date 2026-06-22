#include "real_sec_adapter.h"
#include "mock_sec_service.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace tbox::diag;
using namespace tbox::sec;
using ::testing::Return;
using ::testing::_;

class RealSecAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_service_ = std::make_shared<MockSecService>();
        adapter_ = std::make_unique<RealSecAdapter>(mock_service_);
    }

    std::shared_ptr<MockSecService> mock_service_;
    std::unique_ptr<RealSecAdapter> adapter_;
};

TEST_F(RealSecAdapterTest, IsAvailableWhenInitialized) {
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(true));
    EXPECT_TRUE(adapter_->is_available());
}

TEST_F(RealSecAdapterTest, IsNotAvailableWhenNotInitialized) {
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(false));
    EXPECT_FALSE(adapter_->is_available());
}

TEST_F(RealSecAdapterTest, GenerateKeyPairSuccess) {
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_service_, generate_key_pair())
        .WillOnce(Return(ErrorCode::SUCCESS));
    EXPECT_TRUE(adapter_->generate_key_pair());
}

TEST_F(RealSecAdapterTest, GenerateKeyPairFailure) {
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_service_, generate_key_pair())
        .WillOnce(Return(ErrorCode::KEY_GENERATION_FAILED));
    EXPECT_FALSE(adapter_->generate_key_pair());
}

TEST_F(RealSecAdapterTest, GetCsrSuccess) {
    std::vector<uint8_t> csr_der;
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_service_, get_csr(_))
        .WillOnce(Return(ErrorCode::SUCCESS));
    EXPECT_TRUE(adapter_->get_csr(csr_der));
}

TEST_F(RealSecAdapterTest, GetCsrFailure) {
    std::vector<uint8_t> csr_der;
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_service_, get_csr(_))
        .WillOnce(Return(ErrorCode::CSR_BUILD_FAILED));
    EXPECT_FALSE(adapter_->get_csr(csr_der));
}

TEST_F(RealSecAdapterTest, SubmitCsrSuccess) {
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_service_, submit_csr())
        .WillOnce(Return(ErrorCode::SUCCESS));
    EXPECT_TRUE(adapter_->submit_csr());
}

TEST_F(RealSecAdapterTest, SubmitCsrFailure) {
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_service_, submit_csr())
        .WillOnce(Return(ErrorCode::PKI_REJECTED));
    EXPECT_FALSE(adapter_->submit_csr());
}

TEST_F(RealSecAdapterTest, InjectCertificateSuccess) {
    std::vector<uint8_t> cert_der = {0x30, 0x82, 0x01, 0x00};
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_service_, inject_certificate(cert_der))
        .WillOnce(Return(ErrorCode::SUCCESS));
    EXPECT_TRUE(adapter_->inject_certificate(cert_der));
}

TEST_F(RealSecAdapterTest, InjectCertificateFailure) {
    std::vector<uint8_t> cert_der = {0x30, 0x82, 0x01, 0x00};
    EXPECT_CALL(*mock_service_, is_initialized())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_service_, inject_certificate(cert_der))
        .WillOnce(Return(ErrorCode::CERT_INSTALL_FAILED));
    EXPECT_FALSE(adapter_->inject_certificate(cert_der));
}
