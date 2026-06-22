#include <gtest/gtest.h>
#include "security_access.h"
#include "mock_sec_interface.h"
#include "constants.h"

namespace tbox {
namespace diag {
namespace testing {

class SecurityAccessTest : public ::testing::Test {
protected:
    void SetUp() override {
        mock_sec = std::make_shared<MockSecInterface>();
        sec_access = std::make_unique<SecurityAccess>(mock_sec);
    }

    std::shared_ptr<MockSecInterface> mock_sec;
    std::unique_ptr<SecurityAccess> sec_access;
};

TEST_F(SecurityAccessTest, RequestSeedSuccess) {
    std::vector<uint8_t> seed;
    auto result = sec_access->request_seed(UdsSecurityLevel::LEVEL_27, seed);
    EXPECT_EQ(result, DiagErrorCode::SUCCESS);
    EXPECT_FALSE(seed.empty());
}

TEST_F(SecurityAccessTest, RequestSeedSecUnavailable) {
    mock_sec->set_available(false);

    std::vector<uint8_t> seed;
    auto result = sec_access->request_seed(UdsSecurityLevel::LEVEL_27, seed);
    EXPECT_EQ(result, DiagErrorCode::SEC_UNAVAILABLE);
}

TEST_F(SecurityAccessTest, SendKeySuccess) {
    mock_sec->set_accept_all_keys(true);

    // First request seed
    std::vector<uint8_t> seed;
    sec_access->request_seed(UdsSecurityLevel::LEVEL_27, seed);

    // Then send key
    std::vector<uint8_t> key = {0xAA, 0xBB, 0xCC, 0xDD};
    auto result = sec_access->send_key(UdsSecurityLevel::LEVEL_27, key);
    EXPECT_EQ(result, DiagErrorCode::SUCCESS);

    EXPECT_TRUE(sec_access->is_unlocked(UdsSecurityLevel::LEVEL_27));
}

TEST_F(SecurityAccessTest, SendKeyWithoutSeedRequest) {
    std::vector<uint8_t> key = {0xAA, 0xBB, 0xCC, 0xDD};
    auto result = sec_access->send_key(UdsSecurityLevel::LEVEL_27, key);
    EXPECT_EQ(result, DiagErrorCode::SECURITY_ACCESS_DENIED);
}

TEST_F(SecurityAccessTest, SendKeyInvalidKey) {
    mock_sec->set_accept_all_keys(false);
    mock_sec->set_expected_key(UdsSecurityLevel::LEVEL_27, {0x01, 0x02, 0x03, 0x04});

    std::vector<uint8_t> seed;
    sec_access->request_seed(UdsSecurityLevel::LEVEL_27, seed);

    std::vector<uint8_t> wrong_key = {0xFF, 0xFF, 0xFF, 0xFF};
    auto result = sec_access->send_key(UdsSecurityLevel::LEVEL_27, wrong_key);
    EXPECT_EQ(result, DiagErrorCode::SECURITY_ACCESS_DENIED);

    EXPECT_FALSE(sec_access->is_unlocked(UdsSecurityLevel::LEVEL_27));
}

TEST_F(SecurityAccessTest, LockoutAfterMaxAttempts) {
    mock_sec->set_accept_all_keys(false);

    for (uint32_t i = 0; i < SecurityConfig::MAX_ATTEMPTS; i++) {
        std::vector<uint8_t> seed;
        sec_access->request_seed(UdsSecurityLevel::LEVEL_27, seed);

        std::vector<uint8_t> wrong_key = {0xFF};
        sec_access->send_key(UdsSecurityLevel::LEVEL_27, wrong_key);
    }

    EXPECT_EQ(sec_access->get_attempt_count(UdsSecurityLevel::LEVEL_27),
              SecurityConfig::MAX_ATTEMPTS);

    // Should be locked out now
    std::vector<uint8_t> seed;
    auto result = sec_access->request_seed(UdsSecurityLevel::LEVEL_27, seed);
    EXPECT_EQ(result, DiagErrorCode::SECURITY_ACCESS_DENIED);
}

TEST_F(SecurityAccessTest, IsUnlockedDefault) {
    EXPECT_FALSE(sec_access->is_unlocked(UdsSecurityLevel::LEVEL_27));
}

TEST_F(SecurityAccessTest, Lock) {
    mock_sec->set_accept_all_keys(true);

    std::vector<uint8_t> seed;
    sec_access->request_seed(UdsSecurityLevel::LEVEL_27, seed);

    std::vector<uint8_t> key = {0xAA};
    sec_access->send_key(UdsSecurityLevel::LEVEL_27, key);

    EXPECT_TRUE(sec_access->is_unlocked(UdsSecurityLevel::LEVEL_27));

    sec_access->lock(UdsSecurityLevel::LEVEL_27);
    EXPECT_FALSE(sec_access->is_unlocked(UdsSecurityLevel::LEVEL_27));
}

TEST_F(SecurityAccessTest, Reset) {
    mock_sec->set_accept_all_keys(true);

    std::vector<uint8_t> seed;
    sec_access->request_seed(UdsSecurityLevel::LEVEL_27, seed);

    std::vector<uint8_t> key = {0xAA};
    sec_access->send_key(UdsSecurityLevel::LEVEL_27, key);

    sec_access->reset();
    EXPECT_FALSE(sec_access->is_unlocked(UdsSecurityLevel::LEVEL_27));
    EXPECT_EQ(sec_access->get_attempt_count(UdsSecurityLevel::LEVEL_27), 0);
}

TEST_F(SecurityAccessTest, SecUnavailableForSendKey) {
    std::vector<uint8_t> seed;
    sec_access->request_seed(UdsSecurityLevel::LEVEL_27, seed);

    mock_sec->set_available(false);

    std::vector<uint8_t> key = {0xAA};
    auto result = sec_access->send_key(UdsSecurityLevel::LEVEL_27, key);
    EXPECT_EQ(result, DiagErrorCode::SEC_UNAVAILABLE);
}

} // namespace testing
} // namespace diag
} // namespace tbox
