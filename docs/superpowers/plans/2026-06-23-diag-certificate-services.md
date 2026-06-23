# DIAG 证书服务实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 DIAG 服务中暴露3个独立的 UDS 服务，用于支持诊断仪触发证书申请流程

**Architecture:** 扩展现有 ServiceDispatcher，添加3个新的处理函数，使用 ROUTINE_CONTROL (0x31) + 不同 RID 实现分步控制

**Tech Stack:** C++, UDS (ISO 14229), CMake

---

## 文件结构

### 需要修改的文件
- `include/constants.h` - 添加新的 RID 常量
- `include/sec_interface.h` - 移除 apply_certificate() 方法
- `include/real_sec_adapter.h` - 移除 apply_certificate() 声明
- `src/real_sec_adapter.cpp` - 移除 apply_certificate() 实现
- `include/service_dispatcher.h` - 添加新的处理函数声明
- `src/service_dispatcher.cpp` - 实现新的处理函数和路由逻辑

### 需要创建的文件
- 无

### 需要检查的测试文件
- `tests/` - 更新相关测试用例

---

## 任务分解

### Task 1: 修改 constants.h - 添加新的 RID 常量

**Files:**
- Modify: `include/constants.h:38-41`

- [ ] **Step 1: 修改 RID 常量定义**

```cpp
// RID definitions
namespace Rid {
    constexpr uint16_t WRITE_VIN_ROUTINE = 0xFF00;
    constexpr uint16_t GENERATE_KEY_PAIR = 0xFF01;  // 新增
    constexpr uint16_t READ_CSR = 0xFF02;           // 新增
    constexpr uint16_t INJECT_CERTIFICATE = 0xFF03; // 新增
}
```

- [ ] **Step 2: 验证修改**

检查 `constants.h` 文件，确保 RID 常量已正确添加。

- [ ] **Step 3: Commit**

```bash
git add include/constants.h
git commit -m "feat: add new RID constants for certificate services"
```

---

### Task 2: 修改 sec_interface.h - 移除 apply_certificate() 方法

**Files:**
- Modify: `include/sec_interface.h:22-27`

- [ ] **Step 1: 移除 apply_certificate() 方法声明**

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
    // virtual tbox::sec::ErrorCode apply_certificate() = 0;
};
```

- [ ] **Step 2: 验证修改**

检查 `sec_interface.h` 文件，确保 `apply_certificate()` 方法已移除。

- [ ] **Step 3: Commit**

```bash
git add include/sec_interface.h
git commit -m "refactor: remove apply_certificate() from SecInterface"
```

---

### Task 3: 修改 real_sec_adapter.h - 移除 apply_certificate() 声明

**Files:**
- Modify: `include/real_sec_adapter.h:24-25`

- [ ] **Step 1: 移除 apply_certificate() 声明**

```cpp
class RealSecAdapter : public SecInterface {
public:
    explicit RealSecAdapter(std::shared_ptr<sec::SecService> service);
    ~RealSecAdapter() override = default;

    // 现有方法
    bool get_seed(uint8_t level, std::vector<uint8_t>& seed) override;
    bool verify_key(uint8_t level, const std::vector<uint8_t>& key) override;
    bool is_available() const override;

    // 证书相关方法（保留）
    bool generate_key_pair() override;
    bool get_csr(std::vector<uint8_t>& csr_der) override;
    bool inject_certificate(const std::vector<uint8_t>& cert_der) override;

    // 移除 apply_certificate()
    // tbox::sec::ErrorCode apply_certificate() override;

private:
    std::shared_ptr<sec::SecService> service_;
};
```

- [ ] **Step 2: 验证修改**

检查 `real_sec_adapter.h` 文件，确保 `apply_certificate()` 声明已移除。

- [ ] **Step 3: Commit**

```bash
git add include/real_sec_adapter.h
git commit -m "refactor: remove apply_certificate() from RealSecAdapter header"
```

---

### Task 4: 修改 real_sec_adapter.cpp - 移除 apply_certificate() 实现

**Files:**
- Modify: `src/real_sec_adapter.cpp:116-124`

- [ ] **Step 1: 移除 apply_certificate() 实现**

```cpp
// 移除以下代码：
// tbox::sec::ErrorCode RealSecAdapter::apply_certificate() {
//     if (!is_available()) {
//         std::cerr << "[REAL-SEC] Service not available" << std::endl;
//         return tbox::sec::ErrorCode::NOT_INITIALIZED;
//     }
//
//     std::cout << "[REAL-SEC] apply_certificate" << std::endl;
//     return service_->apply_certificate();
// }
```

- [ ] **Step 2: 验证修改**

检查 `real_sec_adapter.cpp` 文件，确保 `apply_certificate()` 实现已移除。

- [ ] **Step 3: Commit**

```bash
git add src/real_sec_adapter.cpp
git commit -m "refactor: remove apply_certificate() from RealSecAdapter implementation"
```

---

### Task 5: 修改 service_dispatcher.h - 添加新的处理函数声明

**Files:**
- Modify: `include/service_dispatcher.h:36-37`

- [ ] **Step 1: 添加新的处理函数声明**

```cpp
class ServiceDispatcher {
public:
    ServiceDispatcher(std::shared_ptr<ProvInterface> prov,
                      std::shared_ptr<SecInterface> sec,
                      std::shared_ptr<SessionManager> session_mgr,
                      std::shared_ptr<SecurityAccess> security_access);
    virtual ~ServiceDispatcher() = default;

    virtual DiagResponse dispatch(const DiagRequest& request);

    virtual void register_route(uint8_t service_id, uint16_t did_or_rid,
                                const std::string& downstream, bool requires_unlock);

protected:
    DiagResponse handle_session_control(const DiagRequest& request);
    DiagResponse handle_tester_present(const DiagRequest& request);
    DiagResponse handle_security_access(const DiagRequest& request);
    DiagResponse handle_routine_control(const DiagRequest& request);
    DiagResponse handle_read_data_by_identifier(const DiagRequest& request);

    // 新增处理函数
    DiagResponse handle_generate_key_pair(const DiagRequest& request);
    DiagResponse handle_read_csr(const DiagRequest& request);
    DiagResponse handle_inject_certificate(const DiagRequest& request);

    // 移除 handle_certificate_request()
    // DiagResponse handle_certificate_request(const DiagRequest& request);

    DiagResponse create_positive_response(uint8_t service_id, uint8_t sub_function,
                                          const std::vector<uint8_t>& data);
    DiagResponse create_negative_response(uint8_t service_id, uint8_t nrc,
                                          const std::string& diag_error_code);

    bool check_security_required(uint8_t service_id, uint16_t did_or_rid);

    std::shared_ptr<ProvInterface> prov_;
    std::shared_ptr<SecInterface> sec_;
    std::shared_ptr<SessionManager> session_mgr_;
    std::shared_ptr<SecurityAccess> security_access_;
    std::vector<RoutingEntry> routing_table_;
};
```

- [ ] **Step 2: 验证修改**

检查 `service_dispatcher.h` 文件，确保新的处理函数声明已添加，`handle_certificate_request()` 声明已移除。

- [ ] **Step 3: Commit**

```bash
git add include/service_dispatcher.h
git commit -m "refactor: update ServiceDispatcher header with new certificate handlers"
```

---

### Task 6: 修改 service_dispatcher.cpp - 实现新的处理函数和路由逻辑

**Files:**
- Modify: `src/service_dispatcher.cpp:120-175`

- [ ] **Step 1: 修改 handle_routine_control() 路由逻辑**

```cpp
DiagResponse ServiceDispatcher::handle_routine_control(const DiagRequest& request) {
    std::cout << "[DIAG] RoutineControl rid=0x" << std::hex << request.did_or_rid
              << " payload_size=" << std::dec << request.payload.size() << std::endl;

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

    // 检查安全访问
    if (!security_access_->is_unlocked(UdsSecurityLevel::LEVEL_27)) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::SECURITY_ACCESS_DENIED,
                                        error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED));
    }

    // 检查 PROV 可用性
    if (!prov_ || !prov_->is_available()) {
        return create_negative_response(UdsService::ROUTINE_CONTROL,
                                        Nrc::CONDITIONS_NOT_CORRECT,
                                        error_code_to_string(DiagErrorCode::PROV_UNAVAILABLE));
    }

    if (rid == Rid::WRITE_VIN_ROUTINE) {
        // Extract VIN from payload (17 bytes)
        if (request.payload.size() < 17) {
            std::cout << "[DIAG] RoutineControl NRC0x13: payload_size=" << std::dec << request.payload.size()
                      << " (need >= 17)" << std::endl;
            return create_negative_response(UdsService::ROUTINE_CONTROL,
                                            Nrc::INCORRECT_MESSAGE_LENGTH,
                                            error_code_to_string(DiagErrorCode::INVALID_REQUEST_FORMAT));
        }

        std::string vin(request.payload.begin(), request.payload.begin() + 17);
        std::vector<uint8_t> extra_payload(request.payload.begin() + 17, request.payload.end());

        auto result = prov_->write_vin(vin, extra_payload);
        if (result != DiagErrorCode::SUCCESS) {
            uint8_t nrc = Nrc::GENERAL_PROGRAMMING_FAILURE;
            return create_negative_response(UdsService::ROUTINE_CONTROL, nrc,
                                            error_code_to_string(result));
        }

        // ISO 14229: positive response = [controlType] [routineId(2)] [statusRecord...]
        std::vector<uint8_t> rid_echo = {
            static_cast<uint8_t>((request.did_or_rid >> 8) & 0xFF),
            static_cast<uint8_t>(request.did_or_rid & 0xFF)
        };
        return create_positive_response(UdsService::ROUTINE_CONTROL,
                                        request.sub_function, rid_echo);
    }

    return create_negative_response(UdsService::ROUTINE_CONTROL,
                                    Nrc::REQUEST_OUT_OF_RANGE,
                                    "DIAG-1004");
}
```

- [ ] **Step 2: 实现 handle_generate_key_pair()**

```cpp
DiagResponse ServiceDispatcher::handle_generate_key_pair(const DiagRequest& request) {
    std::cout << "[DIAG] GenerateKeyPair rid=0x" << std::hex << request.did_or_rid << std::endl;

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
```

- [ ] **Step 3: 实现 handle_read_csr()**

```cpp
DiagResponse ServiceDispatcher::handle_read_csr(const DiagRequest& request) {
    std::cout << "[DIAG] ReadCSR rid=0x" << std::hex << request.did_or_rid << std::endl;

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
```

- [ ] **Step 4: 实现 handle_inject_certificate()**

```cpp
DiagResponse ServiceDispatcher::handle_inject_certificate(const DiagRequest& request) {
    std::cout << "[DIAG] InjectCertificate rid=0x" << std::hex << request.did_or_rid
              << " payload_size=" << std::dec << request.payload.size() << std::endl;

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

- [ ] **Step 5: 移除 handle_certificate_request() 实现**

```cpp
// 移除以下代码：
// DiagResponse ServiceDispatcher::handle_certificate_request(const DiagRequest& request) {
//     ...
// }
```

- [ ] **Step 6: 验证修改**

检查 `service_dispatcher.cpp` 文件，确保：
1. 新的处理函数已实现
2. `handle_certificate_request()` 已移除
3. 路由逻辑已更新

- [ ] **Step 7: Commit**

```bash
git add src/service_dispatcher.cpp
git commit -m "feat: implement new certificate handlers in ServiceDispatcher"
```

---

### Task 7: 更新测试用例

**Files:**
- Test: `tests/` - 查找并更新相关测试用例

- [ ] **Step 1: 查找相关测试文件**

```bash
find tests/ -name "*.cpp" -o -name "*.h" | xargs grep -l "apply_certificate\|CERTIFICATE_REQUEST"
```

- [ ] **Step 2: 更新测试用例**

根据找到的测试文件，更新测试用例：
1. 移除 `apply_certificate()` 相关测试
2. 添加新的处理函数测试

- [ ] **Step 3: 运行测试**

```bash
cd build && cmake .. && make && ctest
```

- [ ] **Step 4: Commit**

```bash
git add tests/
git commit -m "test: update tests for new certificate services"
```

---

### Task 8: 最终验证

**Files:**
- 无

- [ ] **Step 1: 编译项目**

```bash
cd build && cmake .. && make
```

- [ ] **Step 2: 运行所有测试**

```bash
cd build && ctest
```

- [ ] **Step 3: 检查代码风格**

```bash
# 如果有 clang-format 或其他代码风格检查工具
find src/ include/ -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run
```

- [ ] **Step 4: 最终 Commit**

```bash
git add -A
git commit -m "feat: complete DIAG certificate services implementation"
```

---

## 自检清单

1. **规格覆盖：** 所有3个UDS服务都已实现
2. **占位符扫描：** 无 TBD、TODO 或占位符
3. **类型一致性：** 方法签名和属性名称一致
