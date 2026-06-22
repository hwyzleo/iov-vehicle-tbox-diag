#include <gtest/gtest.h>
#include "session_manager.h"
#include "constants.h"

namespace tbox {
namespace diag {
namespace testing {

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = std::make_unique<SessionManager>();
    }

    std::unique_ptr<SessionManager> mgr;
};

TEST_F(SessionManagerTest, InitialState) {
    auto session = mgr->get_session();
    EXPECT_EQ(session.session_type, SessionType::DEFAULT);
    EXPECT_EQ(session.state, SessionState::IDLE);
    EXPECT_FALSE(session.security_unlocked);
}

TEST_F(SessionManagerTest, SwitchToExtendedSession) {
    auto result = mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);
    EXPECT_EQ(result, DiagErrorCode::SUCCESS);

    auto session = mgr->get_session();
    EXPECT_EQ(session.session_type, SessionType::EXTENDED);
    EXPECT_EQ(session.state, SessionState::ACTIVE);
    EXPECT_EQ(session.source_address, 0x0010);
}

TEST_F(SessionManagerTest, SwitchToProgrammingSession) {
    auto result = mgr->switch_session(UdsSession::PROGRAMMING, 0x0010, TransportType::DOIP);
    EXPECT_EQ(result, DiagErrorCode::SUCCESS);

    auto session = mgr->get_session();
    EXPECT_EQ(session.session_type, SessionType::PROGRAMMING);
}

TEST_F(SessionManagerTest, SwitchToDefaultSession) {
    mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);
    auto result = mgr->switch_session(UdsSession::DEFAULT, 0x0010, TransportType::DOIP);
    EXPECT_EQ(result, DiagErrorCode::SUCCESS);

    auto session = mgr->get_session();
    EXPECT_EQ(session.session_type, SessionType::DEFAULT);
    EXPECT_FALSE(session.security_unlocked);
}

TEST_F(SessionManagerTest, SingleTesterMutex) {
    mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);

    // Different tester should be rejected
    auto result = mgr->switch_session(UdsSession::EXTENDED, 0x0020, TransportType::DOIP);
    EXPECT_EQ(result, DiagErrorCode::SESSION_STATE_NOT_ALLOWED);
}

TEST_F(SessionManagerTest, SameTesterCanSwitch) {
    mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);

    auto result = mgr->switch_session(UdsSession::PROGRAMMING, 0x0010, TransportType::DOIP);
    EXPECT_EQ(result, DiagErrorCode::SUCCESS);
}

TEST_F(SessionManagerTest, TesterPresent) {
    mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);

    auto result = mgr->handle_tester_present(0x0010);
    EXPECT_EQ(result, DiagErrorCode::SUCCESS);
}

TEST_F(SessionManagerTest, TesterPresentFromWrongTester) {
    mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);

    auto result = mgr->handle_tester_present(0x0020);
    EXPECT_EQ(result, DiagErrorCode::SESSION_STATE_NOT_ALLOWED);
}

TEST_F(SessionManagerTest, TesterPresentInDefaultSession) {
    // Default session doesn't require TesterPresent but should still work
    auto result = mgr->handle_tester_present(0x0010);
    EXPECT_EQ(result, DiagErrorCode::SESSION_STATE_NOT_ALLOWED);
}

TEST_F(SessionManagerTest, ReleaseSession) {
    mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);
    mgr->release_session();

    auto session = mgr->get_session();
    EXPECT_EQ(session.session_type, SessionType::DEFAULT);
    EXPECT_EQ(session.state, SessionState::IDLE);
    EXPECT_FALSE(session.security_unlocked);
    EXPECT_EQ(session.source_address, 0);
}

TEST_F(SessionManagerTest, IsSessionActive) {
    EXPECT_FALSE(mgr->is_session_active());

    mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);
    EXPECT_TRUE(mgr->is_session_active());
}

TEST_F(SessionManagerTest, IsInNonDefaultSession) {
    EXPECT_FALSE(mgr->is_in_non_default_session());

    mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);
    EXPECT_TRUE(mgr->is_in_non_default_session());

    mgr->switch_session(UdsSession::DEFAULT, 0x0010, TransportType::DOIP);
    EXPECT_FALSE(mgr->is_in_non_default_session());
}

TEST_F(SessionManagerTest, ResetToDefault) {
    mgr->switch_session(UdsSession::EXTENDED, 0x0010, TransportType::DOIP);
    mgr->reset_to_default();

    auto session = mgr->get_session();
    EXPECT_EQ(session.session_type, SessionType::DEFAULT);
    EXPECT_FALSE(session.security_unlocked);
}

} // namespace testing
} // namespace diag
} // namespace tbox
