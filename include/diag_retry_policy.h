#pragma once

#include <cstdint>

namespace tbox {
namespace diag {

/// DIAG 内部 IPC 方法重试策略
///
/// framework-ipc Client 提供传输失败后的单次重连能力，
/// DiagRetryPolicy 按 method 分类决定是否允许自动重放：
///
/// - 只读/幂等：允许一次重试
/// - 一次性/消费型：禁止自动重放
/// - 写入型：默认禁止
class DiagRetryPolicy {
public:
    /// 方法安全类别
    enum class Category : uint8_t {
        kReadOnly,  ///< 只读/幂等，允许一次重试
        kOneShot,   ///< 一次性/消费型，禁止重放
        kWrite      ///< 写入型，默认禁止
    };

    /// 返回指定 method_id 的安全类别
    static Category categorize(uint32_t method_id);

    /// 传输失败后是否允许自动重试
    static bool should_retry(uint32_t method_id);
};

} // namespace diag
} // namespace tbox
