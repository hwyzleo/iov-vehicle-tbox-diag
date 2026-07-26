#include <gtest/gtest.h>
#include "diag_context.h"
#include "log.h"

namespace tbox::diag {
namespace {

class DiagContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        tbox::fw::log::LogConfig config;
        config.level = tbox::fw::log::LogLevel::kDebug;
        auto result = tbox::fw::log::Logger::init("diag_context_test", config);
        ASSERT_EQ(result.error, tbox::fw::log::LogError::kOk);
    }
};

TEST_F(DiagContextTest, GenerateRequestIdFormat) {
    // 验证 request_id 格式：diag-<timestamp>-<random>
    std::string request_id = generate_request_id();
    
    EXPECT_FALSE(request_id.empty());
    EXPECT_EQ(request_id.find("diag-"), 0u);
    EXPECT_GT(request_id.length(), 10u);
}

TEST_F(DiagContextTest, GenerateRequestIdUnique) {
    // 验证生成的 request_id 是唯一的
    std::string id1 = generate_request_id();
    std::string id2 = generate_request_id();
    
    EXPECT_NE(id1, id2);
}

TEST_F(DiagContextTest, GenerateTraceIdFormat) {
    // 验证 trace_id 格式：diag-trace-<timestamp>-<random>
    std::string trace_id = generate_trace_id();
    
    EXPECT_FALSE(trace_id.empty());
    EXPECT_EQ(trace_id.find("diag-trace-"), 0u);
    EXPECT_GT(trace_id.length(), 15u);
}

TEST_F(DiagContextTest, GenerateTraceIdUnique) {
    // 验证生成的 trace_id 是唯一的
    std::string id1 = generate_trace_id();
    std::string id2 = generate_trace_id();
    
    EXPECT_NE(id1, id2);
}

TEST_F(DiagContextTest, HashSourceAddressConsistent) {
    // 验证相同输入产生相同哈希
    uint16_t source = 0x1234;
    std::string hash1 = hash_source_address(source);
    std::string hash2 = hash_source_address(source);
    
    EXPECT_EQ(hash1, hash2);
}

TEST_F(DiagContextTest, HashSourceAddressDifferent) {
    // 验证不同输入产生不同哈希
    std::string hash1 = hash_source_address(0x1234);
    std::string hash2 = hash_source_address(0x5678);
    
    EXPECT_NE(hash1, hash2);
}

TEST_F(DiagContextTest, HashSourceAddressLength) {
    // 验证哈希长度限制
    std::string hash = hash_source_address(0xFFFF);
    
    EXPECT_FALSE(hash.empty());
    EXPECT_LE(hash.length(), 16u);
}

TEST_F(DiagContextTest, MakeContextScopeSetsContext) {
    // 验证 ContextScope 正确设置上下文
    std::string trace_id = "trace-123";
    std::string request_id = "req-456";
    std::string session_id = "sess-789";
    
    {
        auto scope = make_context_scope(trace_id, request_id, session_id);
        
        auto* ctx = tbox::fw::log::ContextScope::current();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->trace_id, trace_id);
        EXPECT_EQ(ctx->request_id, request_id);
        EXPECT_EQ(ctx->session_id, session_id);
    }
    
    // 离开作用域后上下文应被清理
    auto* ctx = tbox::fw::log::ContextScope::current();
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(DiagContextTest, MakeContextScopeWithoutSessionId) {
    // 验证不带 session_id 的 ContextScope
    std::string trace_id = "trace-123";
    std::string request_id = "req-456";
    
    {
        auto scope = make_context_scope(trace_id, request_id);
        
        auto* ctx = tbox::fw::log::ContextScope::current();
        ASSERT_NE(ctx, nullptr);
        EXPECT_EQ(ctx->trace_id, trace_id);
        EXPECT_EQ(ctx->request_id, request_id);
        EXPECT_TRUE(ctx->session_id.empty());
    }
}

} // namespace
} // namespace tbox::diag
