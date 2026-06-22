# iov-vehicle-tbox-diag

TBOX 诊断服务（DIAG）—— 对接外部诊断仪的本地诊断接入层。

## 架构

DIAG 作为 TBOX 侧的 UDS 诊断服务端，接收外部诊断仪（DTE）经本地传输（DoIP / DoCAN）下发的请求，完成诊断会话与安全访问处理，并将 VIN 写入请求路由至 PROV 执行。

- **SessionManager** — 会话状态机（default/extended/programming）、S3 超时、单一 Tester 互斥
- **SecurityAccess** — 0x27 Seed-Key 协议交互，委托 SEC 校验
- **ServiceDispatcher** — UDS 服务分发与路由（0x10/0x3E/0x27/0x31/0x22）
- **NrcMapper** — DIAG 错误码 ↔ UDS NRC 映射
- **TransportAdapter** — DoIP / DoCAN 传输适配器抽象

## 依赖

- C++17
- Google Test 1.14
- yaml-cpp 0.8
- nlohmann_json 3.11

## 构建

```bash
conan install . --output-folder=build --build=missing
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build .
```

## 测试

```bash
cd build
ctest --output-on-failure
```

## 项目结构

```
├── include/              # 头文件
│   ├── constants.h       # UDS 常量、NRC、DID/RID 定义
│   ├── data_models.h     # 数据模型（DiagSession、DiagRequest/Response 等）
│   ├── error_codes.h     # DIAG 错误码枚举
│   ├── session_manager.h # 会话管理器
│   ├── security_access.h # 安全访问处理器
│   ├── service_dispatcher.h # 服务分发器
│   ├── nrc_mapper.h      # NRC 映射器
│   ├── diag_service.h    # DIAG 主服务类
│   ├── transport_adapter.h # 传输适配器接口
│   ├── doip_adapter.h    # DoIP 适配器
│   ├── do_can_adapter.h  # DoCAN 适配器
│   ├── sec_interface.h   # SEC 下游接口
│   └── prov_interface.h  # PROV 下游接口
├── src/                  # 实现文件
├── tests/                # 单元测试与集成测试
└── specs/diag/           # 需求/设计/任务文档
```
