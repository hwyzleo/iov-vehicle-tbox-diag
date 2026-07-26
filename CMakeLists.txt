cmake_minimum_required(VERSION 3.10)
project(TboxDiagService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions("-Wall -lpthread -g")

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
    src/prov_to_sec_adapter.cpp
    src/diag_log_adapter.cpp
    src/diag_context.cpp
)

# 创建diag_service库
add_library(diag_service_lib STATIC ${LIB_SOURCES})
target_include_directories(diag_service_lib PUBLIC ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_SOURCE_DIR}/include ${FRAMEWORK_SOURCE_DIR}/third_party/include)
target_link_libraries(diag_service_lib PUBLIC ProvClient TboxSecLib yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json ${FRAMEWORK_LIB} fmt::fmt)

# 添加RealSecAdapter源文件
add_library(real_sec_adapter
    src/real_sec_adapter.cpp
)

target_include_directories(real_sec_adapter PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(real_sec_adapter
    TboxSecLib
    ${FRAMEWORK_LIB}
)

# 添加SecIpcAdapter源文件
add_library(sec_ipc_adapter
    src/sec_ipc_adapter.cpp
)

target_include_directories(sec_ipc_adapter PUBLIC ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_SOURCE_DIR}/include)
target_link_libraries(sec_ipc_adapter
    SecClient
    ${FRAMEWORK_LIB}
)

add_executable(TboxDiagService src/main.cpp)
target_link_libraries(TboxDiagService PRIVATE diag_service_lib real_sec_adapter ${FRAMEWORK_LIB} fmt::fmt)
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
    tests/test_real_sec_adapter.cpp
    tests/test_certificate_request.cpp
)
target_include_directories(TboxDiagTests PRIVATE ${PROJECT_SOURCE_DIR}/include ${FRAMEWORK_SOURCE_DIR}/include)
target_link_libraries(TboxDiagTests PRIVATE diag_service_lib real_sec_adapter ${FRAMEWORK_LIB} GTest::GTest GTest::gmock GTest::Main)
add_test(NAME TboxDiagTests COMMAND TboxDiagTests)

# 添加测试
add_executable(test_real_sec_adapter
    tests/test_real_sec_adapter.cpp
)

target_link_libraries(test_real_sec_adapter
    real_sec_adapter
    diag_service_lib
    ${FRAMEWORK_LIB}
    GTest::GTest
    GTest::gmock
    GTest::Main
)

add_executable(test_certificate_request
    tests/test_certificate_request.cpp
)

target_link_libraries(test_certificate_request
    diag_service_lib
    GTest::GTest
    GTest::Main
)

# 添加测试到CTest
add_test(NAME RealSecAdapterTest COMMAND test_real_sec_adapter)
add_test(NAME CertificateRequestTest COMMAND test_certificate_request)

# DIAG CLI 工具
add_executable(diag_cli src/diag_cli.cpp)
target_link_libraries(diag_cli PRIVATE diag_service_lib sec_ipc_adapter ${FRAMEWORK_LIB} fmt::fmt)

# Custom test target that excludes sibling project tests (prov/sec)
# These depend on external services and are not part of this project's unit tests
add_custom_target(test_unit
  COMMAND ${CMAKE_CTEST_COMMAND} -E "TboxProvTests$$|TboxSecTests" --output-on-failure
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running unit tests (excluding prov/sec integration tests)"
  VERBATIM
)
