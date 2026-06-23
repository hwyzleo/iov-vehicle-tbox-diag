# DIAG 证书服务设计文档

## 概述

本设计文档描述了如何在 DIAG 服务中暴露3个独立的 UDS 服务，用于支持诊断仪触发证书申请流程。

## 背景

### 当前架构
- `SecInterface` 已定义了所有底层方法：`generate_key_pair()`, `get_csr()`, `inject_certificate()`
- `ServiceDispatcher` 当前通过 `handle_certificate_request()` 只暴露了一个组合操作 `apply_certificate()`
- 使用 `Rid::CERTIFICATE_REQUEST = 0xFF01` 作为 RID

### 问题
当前的组合操作 `apply_certificate()` 无法满足诊断仪分步控制证书申请流程的需求。诊断仪需要：
1. 触发密钥对生成
2. 读回 CSR
3. （工位自己走 MES→OAPI→PKI，不经过 DIAG）
4. 注入证书

## 设计目标

1. 暴露3个独立 UDS 服务，让诊断仪可以分步控制每个操作
2. 使用 `ROUTINE_CONTROL (0x31)` 服务 ID，为每个操作分配不同的 RID
3. 所有服务都需要安全解锁（Level 27）
4. 仅在 Programming Session 下可用
5. 移除现有的组合操作 `apply_certificate()`

## 详细设计

### 1. RID 常量定义

在 `constants.h` 中添加新的 RID：

```cpp
namespace Rid {
    constexpr uint16_t WRITE_VIN_ROUTINE = 0xFF00;
    constexpr uint16_t GENERATE_KEY_PAIR = 0xFF01;  // 新增
    constexpr uint16_t READ_CSR = 0xFF02;           // 新增
    constexpr uint16_t INJECT_CERTIFICATE = 0xFF03; // 新增
}
```

**注意：** 移除 `CERTIFICATE_REQUEST = 0xFF01`

### 2. SecInterface 修改

在 `sec_interface.h` 中移除 `apply_certificate()` 方法：

```cpp
class SecInterface {
public:
    virtual ~SecInterface() = default;

    // 现有方法
    virtual bool get_seed(uint8_t level, std::vector<uint8_t>& seed) = 0;
    virtual bool verify_key(uint8_t level, const std::vector<uint8_t>& key) = 0;
    virtual bool is_available() const = 0;

    // 证书相关方法（保留）
    virtual bool generate_key_pair() = 0;
    virtual bool get_csr(std::vector<uint8_t>& csr_der) = 0;
    virtual bool inject_certificate(const std::vector<uint8_t>& cert_der) = 0;

    // 移除 apply_certificate()
};
```

### 3. RealSecAdapter 修改

在 `real_sec_adapter.h` 和 `real_sec_adapter.cpp` 中移除 `apply_certificate()` 实现。

### 4. ServiceDispatcher 修改

在 `service_dispatcher.h` 中添加新的处理函数声明：

```cpp
class ServiceDispatcher {
    // ... 现有代码 ...

protected:
    // 新增处理函数
    DiagResponse handle_generate_key_pair(const DiagRequest& request);
    DiagResponse handle_read_csr(const DiagRequest& request);
    DiagResponse handle_inject_certificate(const DiagRequest& request);

    // 移除 handle_certificate_request()
};
```

在 `service_dispatcher.cpp` 中：

**修改 `handle_routine_control()` 路由逻辑：**

```cpp
DiagResponse ServiceDispatcher::handle_routine_control(const DiagRequest& request) {
    uint16_t rid = request.did_or_rid;

    // 检查会话类型（证书相关操作只在 Programming Session 下可用）
    if (rid == Rid::GENERATE_KEY_PAIR || rid == Rid::READ_CSR || rid == Rid::INJECT_CERTIFICATE) {
        if (session_mgr_->get_current_session().session_type != SessionType::PROGRAMMING) {
            return create_negative_response(UdsService::ROUTINE_CONTROL,
                                            Nrc::SERVICE_NOT_SUPPORTED_IN_SESSION,
                                            error_code_to_string(DiagErrorCode::SESSION_STATE_NOT_ALLOWED));
        }
    }

    // 新路由
    if (rid == Rid::GENERATE_KEY_PAIR) {
        return handle_generate_key_pair(request);
    }
    if (rid == Rid::READ_CSR) {
        return handle_read_csr(request);
    }
    if (rid == Rid::INJECT_CERTIFICATE) {
        return handle_inject_certificate(request);
    }

    // ... 其他 RID 处理 ...
}
```

**新增处理函数实现：**

```cpp
DiagResponse ServiceDispatcher::handle_generate_key_pair(const DiagRequest& request) {
    // 检查安全访问
    if (!security_access_->is_unlocked(UdsSecurityLevel::LEVEL_27)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::SECURITY_ACCESS_DENIED,
                                        error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED));
    }

    // 检查SEC服务可用性
    if (!sec_ || !sec_->is_available()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(DiagErrorCode::SEC_UNAVAILABLE));
    }

    // 调用SEC服务生成密钥对
    if (!sec_->generate_key_pair()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::GENERAL_PROGRAMMING_FAILURE,
                                        error_code_to_string(DiagErrorCode::CERT_GENERATION_FAILED));
    }

    // 返回正响应
    std::vector<uint8_t> rid_echo = {
        static_cast<uint8_t>((request.did_or_rid >> 8) & 0xFF),
        static_cast<uint8_t>(request.did_or_rid & 0xFF)
    };
    return create_positive_response(UdsService::ROUTINE_CONTROL,
                                    request.sub_function, rid_echo);
}

DiagResponse ServiceDispatcher::handle_read_csr(const DiagRequest& request) {
    // 检查安全访问
    if (!security_access_->is_unlocked(UdsSecurityLevel::LEVEL_27)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::SECURITY_ACCESS_DENIED,
                                        error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED));
    }

    // 检查SEC服务可用性
    if (!sec_ || !sec_->is_available()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(DiagErrorCode::SEC_UNAVAILABLE));
    }

    // 获取CSR
    std::vector<uint8_t> csr_der;
    if (!sec_->get_csr(csr_der)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::GENERAL_PROGRAMMING_FAILURE,
                                        error_code_to_string(DiagErrorCode::CSR_CREATION_FAILED));
    }

    // 返回正响应（包含CSR数据）
    std::vector<uint8_t> response_data = {
        static_cast<uint8_t>((request.did_or_rid >> 8) & 0xFF),
        static_cast<uint8_t>(request.did_or_rid & 0xFF)
    };
    response_data.insert(response_data.end(), csr_der.begin(), csr_der.end());
    return create_positive_response(UdsService::ROUTINE_CONTROL,
                                    request.sub_function, response_data);
}

DiagResponse ServiceDispatcher::handle_inject_certificate(const DiagRequest& request) {
    // 检查安全访问
    if (!security_access_->is_unlocked(UdsSecurityLevel::LEVEL_27)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::SECURITY_ACCESS_DENIED,
                                        error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED));
    }

    // 检查SEC服务可用性
    if (!sec_ || !sec_->is_available()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(DiagErrorCode::SEC_UNAVAILABLE));
    }

    // 从请求中提取证书数据
    if (request.payload.empty()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::INCORRECT_MESSAGE_LENGTH,
                                        error_code_to_string(DiagErrorCode::INVALID_REQUEST_FORMAT));
    }

    // 注入证书
    if (!sec_->inject_certificate(request.payload)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::GENERAL_PROGRAMMING_FAILURE,
                                        error_code_to_string(DiagErrorCode::CERT_INJECTION_FAILED));
    }

    // 返回正响应
    std::vector<uint8_t> rid_echo = {
        static_cast<uint8_t>((request.did_or_rid >> 8) & 0xFF),
        static_cast<uint8_t>(request.did_or_rid & 0xFF)
    };
    return create_positive_response(UdsService::ROUTINE_CONTROL,
                                    request.sub_function, rid_echo);
}
```

### 5. 会话检查

在 `handle_routine_control()` 开头添加会话检查，确保证书相关操作只在 Programming Session 下可用。

### 6. 错误码处理

在 `handle_inject_certificate` 中，需要根据 SEC 返回的错误码映射到 DIAG 错误码。

## 实现步骤

1. 修改 `constants.h`，添加新的 RID 常量
2. 修改 `sec_interface.h`，移除 `apply_certificate()` 方法
3. 修改 `real_sec_adapter.h` 和 `real_sec_adapter.cpp`，移除 `apply_certificate()` 实现
4. 修改 `service_dispatcher.h`，添加新的处理函数声明
5. 修改 `service_dispatcher.cpp`，实现新的处理函数和路由逻辑
6. 更新测试用例

## 测试策略

1. 单元测试：测试每个新的处理函数
2. 集成测试：测试完整的证书申请流程
3. 边界测试：测试错误情况和异常场景

## 风险与缓解

1. **风险**：移除 `apply_certificate()` 可能影响现有功能
   - **缓解**：确保所有使用 `apply_certificate()` 的地方都已更新

2. **风险**：会话检查可能遗漏
   - **缓解**：在 `handle_routine_control()` 开头统一检查

3. **风险**：错误码映射不完整
   - **缓解**：参考现有错误码映射逻辑
