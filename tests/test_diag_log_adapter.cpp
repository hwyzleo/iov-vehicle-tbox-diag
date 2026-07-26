#include <gtest/gtest.h>
#include "diag_log_adapter.h"
#include "diag_log_events.h"

namespace tbox::diag {
namespace {

class DiagLogAdapterTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        tbox::fw::log::LogConfig config;
        config.level = tbox::fw::log::LogLevel::kDebug;
        auto result = DiagLogAdapter::init("diag_adapter_test", config);
        // 忽略初始化错误，因为可能已经被初始化过
    }
};

TEST_F(DiagLogAdapterTest, InitSuccess) {
    // 验证初始化成功
    EXPECT_TRUE(DiagLogAdapter::is_initialized());
}

TEST_F(DiagLogAdapterTest, GetTransportLogger) {
    // 验证可以获取 transport Logger
    auto logger = DiagLogAdapter::transport();
    EXPECT_NO_THROW(logger.info(events::TRANSPORT_CONNECTED, "test message"));
}

TEST_F(DiagLogAdapterTest, GetSessionLogger) {
    // 验证可以获取 session Logger
    auto logger = DiagLogAdapter::session();
    EXPECT_NO_THROW(logger.info(events::SESSION_CHANGED, "test message"));
}

TEST_F(DiagLogAdapterTest, GetUdsRouterLogger) {
    // 验证可以获取 uds_router Logger
    auto logger = DiagLogAdapter::uds_router();
    EXPECT_NO_THROW(logger.info(events::UDS_REQUEST_COMPLETED, "test message"));
}

TEST_F(DiagLogAdapterTest, GetDownstreamLogger) {
    // 验证可以获取 downstream Logger
    auto logger = DiagLogAdapter::downstream();
    EXPECT_NO_THROW(logger.info(events::DOWNSTREAM_CALL_FAILED, "test message"));
}

TEST_F(DiagLogAdapterTest, GetResponseLogger) {
    // 验证可以获取 response Logger
    auto logger = DiagLogAdapter::response();
    EXPECT_NO_THROW(logger.info(events::UDS_RESPONSE_PENDING, "test message"));
}

TEST_F(DiagLogAdapterTest, DoubleInitFails) {
    // 验证重复初始化失败
    tbox::fw::log::LogConfig config;
    config.level = tbox::fw::log::LogLevel::kDebug;
    auto result = DiagLogAdapter::init("diag_test_2", config);
    EXPECT_NE(result.error, tbox::fw::log::LogError::kOk);
}

} // namespace
} // namespace tbox::diag
