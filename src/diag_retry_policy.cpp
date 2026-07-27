#include "diag_retry_policy.h"
#include "diag_ipc_protocol.h"

namespace tbox {
namespace diag {

DiagRetryPolicy::Category DiagRetryPolicy::categorize(uint32_t method_id) {
    switch (static_cast<ipc::MethodId>(method_id)) {
        // 只读/幂等：允许一次重试
        case ipc::MethodId::GET_SERVICE_STATUS:
        case ipc::MethodId::GET_VEHICLE_INFO:
        case ipc::MethodId::IS_TESTER_CONNECTED:
            return Category::kReadOnly;

        default:
            // 未知方法保守处理：禁止重放
            return Category::kOneShot;
    }
}

bool DiagRetryPolicy::should_retry(uint32_t method_id) {
    return categorize(method_id) == Category::kReadOnly;
}

} // namespace diag
} // namespace tbox
