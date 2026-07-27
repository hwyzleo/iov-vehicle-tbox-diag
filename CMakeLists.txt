cmake_minimum_required(VERSION 3.10)
project(TboxDiagService)

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

find_package(yaml-cpp REQUIRED)
include_directories(${YAML_CPP_INCLUDE_DIRS})

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

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

# Framework library (sibling directory) - use pre-built library to avoid spdlog/fmt conflicts
set(FRAMEWORK_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../iov-vehicle-tbox-framework)
set(FRAMEWORK_INCLUDE_DIR ${FRAMEWORK_SOURCE_DIR}/include)
find_library(FRAMEWORK_LIB tbox-framework PATHS ${FRAMEWORK_SOURCE_DIR}/build NO_DEFAULT_PATH)
if(NOT FRAMEWORK_LIB)
    message(FATAL_ERROR "Framework library not found in ${FRAMEWORK_SOURCE_DIR}/build. Please build framework first.")
endif()
message(STATUS "Found framework library: ${FRAMEWORK_LIB}")

# fmt (required by spdlog)
find_package(fmt REQUIRED)

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
target_include_directories(diag_service_lib PUBLIC ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_SOURCE_DIR}/include ${FRAMEWORK_SOURCE_DIR}/third_party/include)
target_link_libraries(diag_service_lib PUBLIC tbox::prov_client yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json ${FRAMEWORK_LIB} fmt::fmt)
if(TBOX_DIAG_USE_FRAMEWORK_IPC)
    target_compile_definitions(diag_service_lib PUBLIC TBOX_DIAG_USE_FRAMEWORK_IPC)
endif()

# 添加RealSecAdapter源文件
# (removed: DIAG now talks to SEC over unix-socket IPC via SecIpcAdapter/SecClient)

# 添加SecIpcAdapter源文件
add_library(sec_ipc_adapter
    src/sec_ipc_adapter.cpp
)

target_include_directories(sec_ipc_adapter PUBLIC ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_SOURCE_DIR}/include)
target_link_libraries(sec_ipc_adapter
    tbox::sec_client
    ${FRAMEWORK_LIB}
)

add_executable(TboxDiagService src/main.cpp)
target_link_libraries(TboxDiagService PRIVATE diag_service_lib sec_ipc_adapter TboxProvLib ${FRAMEWORK_LIB} fmt::fmt)
set_target_properties(TboxDiagService PROPERTIES OUTPUT_NAME tbox_diag)

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
    ${FRAMEWORK_SOURCE_DIR}/include
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
target_include_directories(TboxDiagTests PRIVATE ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_SOURCE_DIR}/include)
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

# DIAG CLI 工具
add_executable(diag_cli src/diag_cli.cpp)
target_link_libraries(diag_cli PRIVATE diag_service_lib sec_ipc_adapter TboxProvLib ${FRAMEWORK_LIB} fmt::fmt)

# ============================================================
# Client SDK library (tbox_diag_client / tbox::diag_client)
# ============================================================
set(CLIENT_SOURCES
    src/diag_client.cpp
    src/diag_retry_policy.cpp
)

add_library(tbox_diag_client STATIC ${CLIENT_SOURCES})
add_library(tbox::diag_client ALIAS tbox_diag_client)

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
target_include_directories(tbox_diag_client PRIVATE ${FRAMEWORK_SOURCE_DIR}/include)
if(TBOX_DIAG_USE_FRAMEWORK_IPC)
    target_compile_definitions(tbox_diag_client PRIVATE TBOX_DIAG_USE_FRAMEWORK_IPC)
endif()

# Install public SDK headers
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/tbox/diag/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tbox/diag
    FILES_MATCHING PATTERN "*.h"
)

# Install client library
install(TARGETS tbox_diag_client
    EXPORT TboxDiagClientTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Export CMake config
install(EXPORT TboxDiagClientTargets
    FILE TboxDiagClientTargets.cmake
    NAMESPACE tbox::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxDiagClient
)

# CMake config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TboxDiagClientConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/TboxDiagClientConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxDiagClient
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/TboxDiagClientConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TboxDiagClient
)

# 添加测试
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
target_include_directories(TboxDiagIpcTests PRIVATE ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_SOURCE_DIR}/include)
target_link_libraries(TboxDiagIpcTests PRIVATE diag_service_lib tbox_diag_client TboxProvLib ${FRAMEWORK_LIB} GTest::GTest GTest::gmock GTest::Main)
add_test(NAME TboxDiagIpcTests COMMAND TboxDiagIpcTests)
