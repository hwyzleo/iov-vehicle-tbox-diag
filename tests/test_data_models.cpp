#include <gtest/gtest.h>
#include "data_models.h"
#include "constants.h"

namespace tbox {
namespace diag {
namespace testing {

TEST(DataModelsTest, DiagSessionDefaultValues) {
    DiagSession session;
    EXPECT_EQ(session.source_address, 0);
    EXPECT_EQ(session.session_type, SessionType::DEFAULT);
    EXPECT_EQ(session.state, SessionState::IDLE);
    EXPECT_FALSE(session.security_unlocked);
}

TEST(DataModelsTest, DiagRequestDefaultValues) {
    DiagRequest request;
    EXPECT_EQ(request.service_id, 0);
    EXPECT_EQ(request.sub_function, 0);
    EXPECT_EQ(request.did_or_rid, 0);
    EXPECT_TRUE(request.payload.empty());
    EXPECT_EQ(request.source_address, 0);
}

TEST(DataModelsTest, DiagResponseDefaultValues) {
    DiagResponse response;
    EXPECT_TRUE(response.positive);
    EXPECT_TRUE(response.payload.empty());
    EXPECT_EQ(response.nrc, 0);
    EXPECT_TRUE(response.diag_error_code.empty());
}

TEST(DataModelsTest, SecurityAccessStateDefaultValues) {
    SecurityAccessState state;
    EXPECT_EQ(state.level, 0);
    EXPECT_FALSE(state.seed_requested);
    EXPECT_FALSE(state.unlocked);
    EXPECT_EQ(state.attempt_count, 0);
    EXPECT_TRUE(state.requested_seed.empty());
}

TEST(DataModelsTest, RoutingEntryCreation) {
    RoutingEntry entry;
    entry.service_id = UdsService::ROUTINE_CONTROL;
    entry.did_or_rid = Rid::WRITE_VIN_ROUTINE;
    entry.downstream = "PROV";
    entry.requires_unlock = true;

    EXPECT_EQ(entry.service_id, 0x31);
    EXPECT_EQ(entry.downstream, "PROV");
    EXPECT_TRUE(entry.requires_unlock);
}

TEST(ConstantsTest, UdsServiceIds) {
    EXPECT_EQ(UdsService::DIAGNOSTIC_SESSION_CONTROL, 0x10);
    EXPECT_EQ(UdsService::TESTER_PRESENT, 0x3E);
    EXPECT_EQ(UdsService::SECURITY_ACCESS, 0x27);
    EXPECT_EQ(UdsService::READ_DATA_BY_IDENTIFIER, 0x22);
    EXPECT_EQ(UdsService::ROUTINE_CONTROL, 0x31);
}

TEST(ConstantsTest, NrcCodes) {
    EXPECT_EQ(Nrc::SERVICE_NOT_SUPPORTED, 0x11);
    EXPECT_EQ(Nrc::CONDITIONS_NOT_CORRECT, 0x22);
    EXPECT_EQ(Nrc::SECURITY_ACCESS_DENIED, 0x33);
    EXPECT_EQ(Nrc::INVALID_KEY, 0x35);
    EXPECT_EQ(Nrc::GENERAL_PROGRAMMING_FAILURE, 0x72);
    EXPECT_EQ(Nrc::RESPONSE_PENDING, 0x78);
}

TEST(ConstantsTest, DidDefinitions) {
    EXPECT_EQ(Did::VIN, 0xF190);
    EXPECT_EQ(Did::BINDING_STATE, 0xF191);
}

TEST(ConstantsTest, RidDefinitions) {
    EXPECT_EQ(Rid::WRITE_VIN_ROUTINE, 0xFF00);
}

TEST(ConstantsTest, TimingParameters) {
    EXPECT_EQ(Timing::P2_DEFAULT, 5000);
    EXPECT_EQ(Timing::P2_STAR, 5000);
    EXPECT_EQ(Timing::S3_DEFAULT, 5000);
}

TEST(ConstantsTest, SecurityConfig) {
    EXPECT_EQ(SecurityConfig::MAX_ATTEMPTS, 3);
    EXPECT_EQ(SecurityConfig::LOCKOUT_DURATION_MS, 10000);
}

} // namespace testing
} // namespace diag
} // namespace tbox
