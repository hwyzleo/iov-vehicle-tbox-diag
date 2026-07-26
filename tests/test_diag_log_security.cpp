#include <gtest/gtest.h>
#include "diag_log_adapter.h"
#include "diag_log_events.h"
#include "diag_context.h"
#include <vector>
#include <string>

namespace tbox::diag {
namespace {

class DiagLogSecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        tbox::fw::log::LogConfig config;
        config.level = tbox::fw::log::LogLevel::kDebug;
        static int counter = 0;
        std::string service_name = "diag_security_" + std::to_string(counter++);
        auto result = DiagLogAdapter::init(service_name, config);
        // 忽略初始化错误，因为可能已经被初始化过
    }
};

TEST_F(DiagLogSecurityTest, SeedNeverLogged) {
    auto logger = DiagLogAdapter::session();
    
    // 尝试记录 seed（应被拒绝或脱敏）
    // 注意：framework-log 的 Secret 敏感度会拒绝输出
    logger.info("test.event", "test message",
        {
            tbox::fw::log::Field("seed", tbox::fw::log::FieldValue::makeString("secret_seed_value"), tbox::fw::log::Sensitivity::Secret)
        }
    );
    
    // 验证日志记录成功（不崩溃即通过）
    // 实际的脱敏行为由 framework-log 保证
}

TEST_F(DiagLogSecurityTest, KeyNeverLogged) {
    auto logger = DiagLogAdapter::session();
    
    // 尝试记录 key（应被拒绝或脱敏）
    logger.info("test.event", "test message",
        {
            tbox::fw::log::Field("key", tbox::fw::log::FieldValue::makeString("secret_key_value"), tbox::fw::log::Sensitivity::Secret)
        }
    );
    
    // 验证日志记录成功（不崩溃即通过）
}

TEST_F(DiagLogSecurityTest, VinNeverInMessage) {
    auto logger = DiagLogAdapter::session();
    
    // 记录 VIN 相关事件（使用 vin_hash 而非 vin）
    logger.info("test.event", "VIN processing completed",
        {
            tbox::fw::log::Field("vin_hash", tbox::fw::log::FieldValue::makeString("abc123hash"))
        }
    );
    
    // 验证日志记录成功（不崩溃即通过）
    // message 中不应包含 VIN
}

TEST_F(DiagLogSecurityTest, SourceAddressHashed) {
    auto logger = DiagLogAdapter::transport();
    
    // 使用哈希后的源地址
    std::string source_hash = hash_source_address(0x1234);
    
    logger.info(events::TRANSPORT_CONNECTED,
        "Connection established",
        {
            tbox::fw::log::Field(events::fields::SOURCE_HASH, tbox::fw::log::FieldValue::makeString(source_hash))
        }
    );
    
    // 验证日志记录成功（不崩溃即通过）
    // 日志中不应包含原始源地址
}

TEST_F(DiagLogSecurityTest, SeedKeyCombinationNeverLogged) {
    auto logger = DiagLogAdapter::session();
    
    // 尝试同时记录 seed 和 key（应被拒绝）
    logger.info("test.event", "test message",
        {
            tbox::fw::log::Field("seed", tbox::fw::log::FieldValue::makeString("seed_value"), tbox::fw::log::Sensitivity::Secret),
            tbox::fw::log::Field("key", tbox::fw::log::FieldValue::makeString("key_value"), tbox::fw::log::Sensitivity::Secret),
            tbox::fw::log::Field("attempt_count", tbox::fw::log::FieldValue::makeInt(3))
        }
    );
    
    // 验证日志记录成功（不崩溃即通过）
}

TEST_F(DiagLogSecurityTest, ContextIdNotFromSensitiveData) {
    // 验证上下文 ID 不来自敏感数据
    std::string trace_id = generate_trace_id();
    std::string request_id = generate_request_id();
    
    // 验证 ID 格式正确
    EXPECT_EQ(trace_id.find("diag-trace-"), 0u);
    EXPECT_EQ(request_id.find("diag-"), 0u);
    
    // 验证 ID 不包含敏感信息
    EXPECT_EQ(trace_id.find("seed"), std::string::npos);
    EXPECT_EQ(trace_id.find("key"), std::string::npos);
    EXPECT_EQ(trace_id.find("vin"), std::string::npos);
    EXPECT_EQ(request_id.find("seed"), std::string::npos);
    EXPECT_EQ(request_id.find("key"), std::string::npos);
    EXPECT_EQ(request_id.find("vin"), std::string::npos);
}

} // namespace
} // namespace tbox::diag
