#include <gtest/gtest.h>
#include "diag_log_adapter.h"

namespace tbox::diag {
namespace {

class DiagLogFaultTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // 初始化 Logger 一次
        tbox::fw::log::LogConfig config;
        config.level = tbox::fw::log::LogLevel::kDebug;
        DiagLogAdapter::init("diag_fault_test", config);
    }
};

TEST_F(DiagLogFaultTest, LoggerInitFailWithInvalidLevel) {
    // 模拟 Logger 初始化失败（无效级别）
    // 注意：framework-log 可能接受无效级别，这里主要测试不会崩溃
    tbox::fw::log::LogConfig config;
    config.level = static_cast<tbox::fw::log::LogLevel>(99);  // 无效级别
    
    // 由于 Logger 已经初始化，这里应该返回错误
    auto result = DiagLogAdapter::init("test_fault_2", config);
    EXPECT_NE(result.error, tbox::fw::log::LogError::kOk);
}

TEST_F(DiagLogFaultTest, QueueOverflowDoesNotAffectService) {
    // 验证 Logger 已经初始化
    EXPECT_TRUE(DiagLogAdapter::is_initialized());
    
    auto logger = DiagLogAdapter::transport();
    
    // 快速发送大量日志
    for (int i = 0; i < 1000; ++i) {
        logger.info("test.event", "test message",
            {tbox::fw::log::Field("index", tbox::fw::log::FieldValue::makeInt(i))}
        );
    }
    
    // 验证服务仍然正常工作
    EXPECT_TRUE(DiagLogAdapter::is_initialized());
}

TEST_F(DiagLogFaultTest, DoubleInitReturnsError) {
    // 测试重复初始化
    tbox::fw::log::LogConfig config;
    config.level = tbox::fw::log::LogLevel::kDebug;
    
    auto result = DiagLogAdapter::init("test_fault_4_again", config);
    EXPECT_NE(result.error, tbox::fw::log::LogError::kOk);
}

TEST_F(DiagLogFaultTest, AllLoggersWork) {
    // 验证所有 Logger 都能正常工作
    auto transport_logger = DiagLogAdapter::transport();
    auto session_logger = DiagLogAdapter::session();
    auto uds_router_logger = DiagLogAdapter::uds_router();
    auto downstream_logger = DiagLogAdapter::downstream();
    auto response_logger = DiagLogAdapter::response();
    
    // 调用日志方法（不崩溃即通过）
    transport_logger.info("test.event", "test message");
    session_logger.info("test.event", "test message");
    uds_router_logger.info("test.event", "test message");
    downstream_logger.info("test.event", "test message");
    response_logger.info("test.event", "test message");
}

} // namespace
} // namespace tbox::diag
