#pragma once

#include "log.h"
#include "log_types.h"
#include <string>

namespace tbox::diag {

/**
 * 创建 ContextScope 的便捷函数
 * 
 * 用法：
 *   auto scope = make_context_scope(trace_id, request_id, session_id);
 *   // scope 离开作用域时自动清理上下文
 */
[[nodiscard]] inline tbox::fw::log::ContextScope make_context_scope(
    const std::string& trace_id,
    const std::string& request_id,
    const std::string& session_id = ""
) {
    tbox::fw::log::LogContext ctx;
    ctx.trace_id = trace_id;
    ctx.request_id = request_id;
    ctx.session_id = session_id;
    return tbox::fw::log::ContextScope(ctx);
}

/**
 * 生成请求 ID
 * 格式: diag-<timestamp>-<random>
 */
std::string generate_request_id();

/**
 * 生成 trace ID（如果上游没有提供）
 * 格式: diag-trace-<timestamp>-<random>
 */
std::string generate_trace_id();

/**
 * 计算源地址哈希（用于脱敏）
 * 使用不可逆摘要，避免泄露真实源地址
 */
std::string hash_source_address(uint16_t source_address);

} // namespace tbox::diag
