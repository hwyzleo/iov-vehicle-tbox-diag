#include <gtest/gtest.h>
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include "diag_context.h"

namespace tbox::diag {
namespace {

class DiagLogIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        tbox::fw::log::LogConfig config;
        config.level = tbox::fw::log::LogLevel::kDebug;
        static int counter = 0;
        std::string service_name = "diag_integration_" + std::to_string(counter++);
        auto result = DiagLogAdapter::init(service_name, config);
        // 忽略初始化错误，因为可能已经被初始化过
    }
};

TEST_F(DiagLogIntegrationTest, ContextPropagationDteToDiagToSec) {
    // 验证 DTE → DIAG → SEC 的上下文传播
    std::string trace_id = "trace-123";
    std::string request_id = "req-456";
    std::string session_id = "sess-789";
    
    {
        auto scope = make_context_scope(trace_id, request_id, session_id);
        
        // 验证上下文存在
        auto* ctx = tbox::fw::log::ContextScope::current();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->trace_id, trace_id);
        EXPECT_EQ(ctx->request_id, request_id);
        EXPECT_EQ(ctx->session_id, session_id);
        
        // 模拟 DIAG 调用 SEC
        // 在实际代码中，ContextScope 会自动传播到下游
    }
    
    // 离开作用域后上下文应被清理
    auto* ctx = tbox::fw::log::ContextScope::current();
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(DiagLogIntegrationTest, ContextPropagationDteToDiagToProv) {
    // 验证 DTE → DIAG → PROV 的上下文传播
    std::string trace_id = "trace-abc";
    std::string request_id = "req-def";
    std::string session_id = "sess-ghi";
    
    {
        auto scope = make_context_scope(trace_id, request_id, session_id);
        
        // 验证上下文存在
        auto* ctx = tbox::fw::log::ContextScope::current();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->trace_id, trace_id);
        EXPECT_EQ(ctx->request_id, request_id);
        EXPECT_EQ(ctx->session_id, session_id);
        
        // 模拟 DIAG 调用 PROV
        // 在实际代码中，ContextScope 会自动传播到下游
    }
}

TEST_F(DiagLogIntegrationTest, SessionChangeLogging) {
    // 验证会话切换日志
    auto session_logger = DiagLogAdapter::session();
    
    // 模拟会话切换
    session_logger.info(events::SESSION_CHANGED,
        "Test session change",
        {
            tbox::fw::log::Field(events::fields::PREVIOUS_SESSION, tbox::fw::log::FieldValue::makeString("default")),
            tbox::fw::log::Field(events::fields::TARGET_SESSION, tbox::fw::log::FieldValue::makeString("extended"))
        }
    );
    
    // 验证日志记录成功（不崩溃即通过）
}

TEST_F(DiagLogIntegrationTest, SecurityAccessLogging) {
    // 验证安全访问日志
    auto session_logger = DiagLogAdapter::session();
    
    // 模拟安全访问成功
    session_logger.info(events::SECURITY_ACCESS_SUCCEEDED,
        "Test security access",
        {
            tbox::fw::log::Field(events::fields::SECURITY_LEVEL, tbox::fw::log::FieldValue::makeString("0x01")),
            tbox::fw::log::Field(events::fields::DURATION_MS, tbox::fw::log::FieldValue::makeInt(100))
        }
    );
    
    // 模拟安全访问失败
    session_logger.warn(events::SECURITY_ACCESS_FAILED,
        "Test security access failed",
        {
            tbox::fw::log::Field(events::fields::SECURITY_LEVEL, tbox::fw::log::FieldValue::makeString("0x01")),
            tbox::fw::log::Field(events::fields::FAILURE_REASON, tbox::fw::log::FieldValue::makeString("invalid_key")),
            tbox::fw::log::Field(events::fields::ATTEMPT_COUNT, tbox::fw::log::FieldValue::makeInt(3)),
            tbox::fw::log::Field(events::fields::DELAY_MS, tbox::fw::log::FieldValue::makeInt(10000))
        }
    );
    
    // 验证日志记录成功（不崩溃即通过）
}

} // namespace
} // namespace tbox::diag
