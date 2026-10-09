cmake_minimum_required(VERSION 3.10)
# VERSION 用于 write_basic_package_version_file（TboxDiagClient SDK package）
project(TboxDiagService VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions("-Wall -lpthread -g")

include(GNUInstallDirs)

# ============================================================
# Build options
# ============================================================
option(TBOX_DIAG_USE_FRAMEWORK_IPC "Use framework-ipc transport (ON) or disabled (OFF)" ON)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    if (APPLE)
        set(LIB_DIR ${PROJECT_SOURCE_DIR}/third_party/lib/arm64-apple-darwin)
    elseif (UNIX AND NOT APPLE)
        set(LIB_DIR ${PROJECT_SOURCE_DIR}/third_party/lib/aarch64-linux-gnu)
    endif ()
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    if (APPLE)
        set(LIB_DIR ${PROJECT_SOURCE_DIR}/third_party/lib/x86_64-apple-darwin)
    elseif (UNIX AND NOT APPLE)
        set(LIB_DIR ${PROJECT_SOURCE_DIR}/third_party/lib/x86_64-linux-gnu)
    endif ()
endif ()

# yaml-cpp / spdlog 由 TBoxFramework 自包含提供（见下方 find_package(TBoxFramework)），
# 不再单独 find_package，避免命中构建机上的 brew / conda 副本造成版本漂移。

# --- GTest（仅 HOST 单元测试；BUILD 目标构建不提供） ---
# REQUIRED -> QUIET：BUILD 的受控交叉构建容器只构建/安装 declared targets
# （TboxDiagService / tbox_diag_client），不提供 GTest。此前的 REQUIRED 让
# tbox-someip-orin 全量 release-set 在 diag configure 阶段直接失败。
# 与 prov / sec 的既有约定一致（find_package(GTest QUIET) + if(GTest_FOUND)）。
find_package(GTest QUIET)
if(GTest_FOUND)
    include_directories(${GTEST_INCLUDE_DIRS})
endif()

find_package(nlohmann_json REQUIRED)

# PROV library (sibling directory)
set(PROV_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../iov-vehicle-tbox-prov)
add_subdirectory(${PROV_SOURCE_DIR} ${CMAKE_BINARY_DIR}/prov EXCLUDE_FROM_ALL)

# SEC library (sibling directory)
set(SEC_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../iov-vehicle-tbox-sec)
add_subdirectory(${SEC_SOURCE_DIR} ${CMAKE_BINARY_DIR}/sec EXCLUDE_FROM_ALL)

# Remove sibling project test registrations from our CTest
# (they depend on external services and are not part of this project's unit tests)
set_property(GLOBAL PROPERTY CTEST_EXCLUDE_FROM_TEST TboxProvTests TboxSecTests)

# Framework library (sibling directory) - 统一通过 find_package 消费
# framework 自包含 yaml-cpp / spdlog，查找前缀来自
# iov-vehicle-tbox-framework/scripts/tbox-env.sh 的 TBOX_PREFIX（见 scripts/build.sh）。
# 兄弟项目 prov / sec 通过 add_subdirectory 引入时也会 find_package，各自已做幂等保护。
if(NOT TARGET TBoxFramework::config)
    # package 名是 TboxFramework（安装文件 TboxFrameworkConfig.cmake），
    # 导出 target 命名空间才是 TBoxFramework::。find_package 在
    # 大小写敏感文件系统（Linux/Orin）上不会把 TBoxFramework 匹配到
    # TboxFrameworkConfig.cmake，必须与 prov/sec/mqtt/tsp 保持一致写法。
    find_package(TboxFramework CONFIG REQUIRED)
endif()
set(FRAMEWORK_LIB TBoxFramework::config TBoxFramework::log TBoxFramework::ipc)
set(FRAMEWORK_INCLUDE_DIR ${TBoxFramework_INCLUDE_DIRS})
message(STATUS "Using TBoxFramework headers: ${FRAMEWORK_INCLUDE_DIR}")

# 不再 find_package(fmt)：spdlog 由 TBoxFramework 提供且自带 bundled fmt，
# 本项目代码亦未直接使用 fmt。

set(LIB_SOURCES
    src/diag_service.cpp
    src/session_manager.cpp
    src/security_access.cpp
    src/service_dispatcher.cpp
    src/nrc_mapper.cpp
    src/error_codes.cpp
    src/uds_codec.cpp
    src/doip_adapter.cpp
    src/do_can_adapter.cpp
    src/real_prov.cpp
    src/diag_log_adapter.cpp
    src/diag_context.cpp
)

if(TBOX_DIAG_USE_FRAMEWORK_IPC)
    list(APPEND LIB_SOURCES
        src/diag_ipc_dispatcher.cpp
        src/diag_retry_policy.cpp
    )
    message(STATUS "DIAG IPC: framework-ipc mode (TBOX_DIAG_USE_FRAMEWORK_IPC=ON)")
else()
    message(STATUS "DIAG IPC: disabled (TBOX_DIAG_USE_FRAMEWORK_IPC=OFF)")
endif()

# 创建diag_service库
add_library(diag_service_lib STATIC ${LIB_SOURCES})
target_include_directories(diag_service_lib PUBLIC ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_INCLUDE_DIR})
target_link_libraries(diag_service_lib PUBLIC tbox::prov_client nlohmann_json::nlohmann_json ${FRAMEWORK_LIB})
if(TBOX_DIAG_USE_FRAMEWORK_IPC)
    target_compile_definitions(diag_service_lib PUBLIC TBOX_DIAG_USE_FRAMEWORK_IPC)
endif()

# 添加RealSecAdapter源文件
# (removed: DIAG now talks to SEC over unix-socket IPC via SecIpcAdapter/SecClient)

# 添加SecIpcAdapter源文件
add_library(sec_ipc_adapter
    src/sec_ipc_adapter.cpp
)

target_include_directories(sec_ipc_adapter PUBLIC ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_INCLUDE_DIR})
target_link_libraries(sec_ipc_adapter
    tbox::sec_client
    ${FRAMEWORK_LIB}
)

add_executable(TboxDiagService src/main.cpp)
target_link_libraries(TboxDiagService PRIVATE diag_service_lib sec_ipc_adapter TboxProvLib ${FRAMEWORK_LIB})
set_target_properties(TboxDiagService PROPERTIES OUTPUT_NAME tbox_diag)

# ============================================================
# Tests (HOST only)
# BUILD 的受控交叉构建容器不提供 GTest，整段跳过；
# 本段只注册测试目标，不影响 TboxDiagService / tbox_diag_client 的安装契约。
# ============================================================
if(GTest_FOUND)
enable_testing()

# DIAG Log Tests (standalone, no diag_service_lib dependency)
add_executable(TboxDiagLogTests
    tests/test_diag_log_events.cpp
    tests/test_diag_context.cpp
    tests/test_diag_log_adapter.cpp
    tests/test_diag_log_integration.cpp
    tests/test_diag_log_fault.cpp
    tests/test_diag_log_security.cpp
    src/diag_context.cpp
    src/diag_log_adapter.cpp
)
target_include_directories(TboxDiagLogTests PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${FRAMEWORK_INCLUDE_DIR}
)
target_link_libraries(TboxDiagLogTests PRIVATE 
    ${FRAMEWORK_LIB}
    GTest::GTest 
    GTest::gmock 
    GTest::Main
)
set_target_properties(TboxDiagLogTests PROPERTIES
    BUILD_RPATH_USE_ORIGIN TRUE
    INSTALL_RPATH_USE_LINK_PATH TRUE
)
add_test(NAME TboxDiagLogTests COMMAND TboxDiagLogTests)

add_executable(TboxDiagTests
    tests/test_session_manager.cpp
    tests/test_security_access.cpp
    tests/test_service_dispatcher.cpp
    tests/test_nrc_mapper.cpp
    tests/test_diag_service.cpp
    tests/test_data_models.cpp
    tests/test_error_codes.cpp
    tests/test_transport_adapter.cpp
    tests/test_integration.cpp
    tests/test_certificate_request.cpp
    tests/test_diag_retry_policy.cpp
    tests/test_diag_ipc_dispatcher.cpp
)
target_include_directories(TboxDiagTests PRIVATE ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_INCLUDE_DIR})
target_link_libraries(TboxDiagTests PRIVATE diag_service_lib TboxProvLib ${FRAMEWORK_LIB} GTest::GTest GTest::gmock GTest::Main)
add_test(NAME TboxDiagTests COMMAND TboxDiagTests)

# 添加测试
add_executable(test_certificate_request
    tests/test_certificate_request.cpp
)

target_link_libraries(test_certificate_request
    diag_service_lib
    GTest::GTest
    GTest::Main
)

# 添加测试到CTest
add_test(NAME CertificateRequestTest COMMAND test_certificate_request)

# 下游 IPC 适配器重连回归测试（SEC / PROV，需链接 sec_ipc_adapter 与 framework-ipc）
add_executable(TboxDiagSecIpcTests
    tests/test_sec_ipc_adapter.cpp
    tests/test_real_prov_reconnect.cpp
)
target_link_libraries(TboxDiagSecIpcTests
    PRIVATE diag_service_lib sec_ipc_adapter TboxProvLib ${FRAMEWORK_LIB}
    GTest::GTest GTest::Main
)
add_test(NAME TboxDiagSecIpcTests COMMAND TboxDiagSecIpcTests)
endif() # GTest_FOUND

# DIAG CLI 工具
add_executable(diag_cli src/diag_cli.cpp)
target_link_libraries(diag_cli PRIVATE diag_service_lib sec_ipc_adapter TboxProvLib ${FRAMEWORK_LIB})

# ============================================================
# Client SDK library (tbox_diag_client / tbox::diag_client)
# ============================================================
set(CLIENT_SOURCES
    src/diag_client.cpp
    src/diag_retry_policy.cpp
)

add_library(tbox_diag_client STATIC ${CLIENT_SOURCES})
add_library(tbox::diag_client ALIAS tbox_diag_client)
# EXPORT_NAME：使安装后的导出 target 也是 tbox::diag_client（与 build-tree ALIAS
# 一致，对齐 prov_client / sec_client / mqtt_client）。缺失时消费方拿到的是
# tbox::tbox_diag_client，SDK 契约与 build-tree 不一致。
set_target_properties(tbox_diag_client PROPERTIES EXPORT_NAME diag_client)

# PUBLIC: only business facade/DTO headers
target_include_directories(tbox_diag_client
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# PRIVATE: framework-ipc link and internal headers
target_link_libraries(tbox_diag_client
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE ${FRAMEWORK_LIB}
)
target_include_directories(tbox_diag_client PRIVATE ${FRAMEWORK_INCLUDE_DIR})
if(TBOX_DIAG_USE_FRAMEWORK_IPC)
    target_compile_definitions(tbox_diag_client PRIVATE TBOX_DIAG_USE_FRAMEWORK_IPC)
endif()

# ============================================================
# Install rules — 两个显式 component（对齐 PROV CR-009 §5.3 / SEC CR-012 §9）
#
#   diag-sdk     -> staging: sdk      (公开头文件 / client 静态库 / CMake package)
#   diag-runtime -> staging: rootfs   (daemon / systemd unit / 非秘密默认配置)
#
# 没有 diag-runtime component 时 BUILD 无法把 DIAG 产物 staging 进 rootfs，
# 设备只能继续沿用遗留的 /opt/tbox/bin/diag 单体产物 —— 这是必须补齐的缺口。
# ============================================================

# --- diag-runtime: daemon binary (/usr/bin/tbox_diag) ---
install(TARGETS TboxDiagService
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT diag-runtime
)

# --- diag-runtime: systemd unit ---
install(FILES packaging/systemd/tbox-diag.service
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system
    COMPONENT diag-runtime
)

# --- diag-runtime: 默认非秘密配置模板 ---
# 仅安装 diag.default.yaml -> /etc/tbox/conf.d/diag.yaml。
# 开发用 config/diag.yaml（含顶层 common: 与 store.root: "."）不安装。
install(FILES config/diag.default.yaml
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/tbox/conf.d
    RENAME diag.yaml
    COMPONENT diag-runtime
)

# --- diag-sdk: public headers ---
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/tbox/diag/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tbox/diag
    COMPONENT diag-sdk
    FILES_MATCHING PATTERN "*.h"
)

# --- diag-sdk: client static library + export ---
install(TARGETS tbox_diag_client
    EXPORT TboxDiagClientTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT diag-sdk
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT diag-sdk
)

# Export CMake config
install(EXPORT TboxDiagClientTargets
    FILE TboxDiagClientTargets.cmake
    NAMESPACE tbox::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxDiagClient
    COMPONENT diag-sdk
)

# CMake config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TboxDiagClientConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/TboxDiagClientConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxDiagClient
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/TboxDiagClientConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/TboxDiagClientConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/TboxDiagClientConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxDiagClient
    COMPONENT diag-sdk
)

# 添加测试
# HOST only：与上方测试段同条件，BUILD 容器无 GTest 时整段跳过。
if(GTest_FOUND)
add_custom_target(test_unit
  COMMAND ${CMAKE_CTEST_COMMAND} -E "TboxProvTests$$|TboxSecTests" --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running unit tests (excluding prov/sec integration tests)"
  VERBATIM
)

# ============================================================
# DIAG IPC integration test (Server + Client loopback)
# ============================================================
add_executable(TboxDiagIpcTests
    tests/test_diag_ipc_integration.cpp
)
target_include_directories(TboxDiagIpcTests PRIVATE ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_INCLUDE_DIR})
target_link_libraries(TboxDiagIpcTests PRIVATE diag_service_lib tbox_diag_client TboxProvLib ${FRAMEWORK_LIB} GTest::GTest GTest::gmock GTest::Main)
add_test(NAME TboxDiagIpcTests COMMAND TboxDiagIpcTests)
endif() # GTest_FOUND
