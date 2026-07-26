#include <gtest/gtest.h>
#include "diag_log_events.h"
#include <regex>

namespace tbox::diag {
namespace {

TEST(DiagLogEventsTest, EventNameFormat) {
    // 验证事件名格式：diag.<module>.<action>
    std::regex event_pattern("^diag\\.[a-z_]+\\.[a-z_.]+$");
    
    EXPECT_TRUE(std::regex_match(events::TRANSPORT_CONNECTED, event_pattern));
    EXPECT_TRUE(std::regex_match(events::TRANSPORT_CONNECTION_FAILED, event_pattern));
    EXPECT_TRUE(std::regex_match(events::SESSION_CHANGED, event_pattern));
    EXPECT_TRUE(std::regex_match(events::SESSION_TIMED_OUT, event_pattern));
    EXPECT_TRUE(std::regex_match(events::SESSION_TESTER_REJECTED, event_pattern));
    EXPECT_TRUE(std::regex_match(events::SECURITY_ACCESS_SUCCEEDED, event_pattern));
    EXPECT_TRUE(std::regex_match(events::SECURITY_ACCESS_FAILED, event_pattern));
    EXPECT_TRUE(std::regex_match(events::UDS_REQUEST_REJECTED, event_pattern));
    EXPECT_TRUE(std::regex_match(events::UDS_REQUEST_COMPLETED, event_pattern));
    EXPECT_TRUE(std::regex_match(events::DOWNSTREAM_CALL_FAILED, event_pattern));
    EXPECT_TRUE(std::regex_match(events::UDS_RESPONSE_PENDING, event_pattern));
}

TEST(DiagLogEventsTest, FieldNameFormat) {
    // 验证字段名格式：小写下划线
    std::regex field_pattern("^[a-z_]+$");
    
    EXPECT_TRUE(std::regex_match(events::fields::TRANSPORT, field_pattern));
    EXPECT_TRUE(std::regex_match(events::fields::SOURCE_HASH, field_pattern));
    EXPECT_TRUE(std::regex_match(events::fields::DURATION_MS, field_pattern));
    EXPECT_TRUE(std::regex_match(events::fields::SERVICE_ID, field_pattern));
    EXPECT_TRUE(std::regex_match(events::fields::TRACE_ID, field_pattern));
    EXPECT_TRUE(std::regex_match(events::fields::REQUEST_ID, field_pattern));
}

TEST(DiagLogEventsTest, EventValuesNotChanged) {
    // 验证事件名值是稳定的检索契约
    EXPECT_STREQ(events::TRANSPORT_CONNECTED, "diag.transport.connected");
    EXPECT_STREQ(events::TRANSPORT_CONNECTION_FAILED, "diag.transport.connection_failed");
    EXPECT_STREQ(events::SESSION_CHANGED, "diag.session.changed");
    EXPECT_STREQ(events::SESSION_TIMED_OUT, "diag.session.timed_out");
    EXPECT_STREQ(events::SESSION_TESTER_REJECTED, "diag.session.tester_rejected");
    EXPECT_STREQ(events::SECURITY_ACCESS_SUCCEEDED, "diag.security_access.succeeded");
    EXPECT_STREQ(events::SECURITY_ACCESS_FAILED, "diag.security_access.failed");
    EXPECT_STREQ(events::UDS_REQUEST_REJECTED, "diag.uds.request.rejected");
    EXPECT_STREQ(events::UDS_REQUEST_COMPLETED, "diag.uds.request.completed");
    EXPECT_STREQ(events::DOWNSTREAM_CALL_FAILED, "diag.downstream.call_failed");
    EXPECT_STREQ(events::UDS_RESPONSE_PENDING, "diag.uds.response_pending");
}

} // namespace
} // namespace tbox::diag
