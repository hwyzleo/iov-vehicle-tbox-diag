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
)

# 创建diag_service库
add_library(diag_service_lib STATIC ${LIB_SOURCES})
target_include_directories(diag_service_lib PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(diag_service_lib PUBLIC TboxProvLib TboxSecLib yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json)

# 添加RealSecAdapter源文件
add_library(real_sec_adapter
    src/real_sec_adapter.cpp
)

target_include_directories(real_sec_adapter PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(real_sec_adapter
    TboxSecLib
)

add_executable(TboxDiagService src/main.cpp)
target_link_libraries(TboxDiagService PRIVATE diag_service_lib real_sec_adapter)

enable_testing()
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
target_include_directories(TboxDiagTests PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(TboxDiagTests PRIVATE diag_service_lib real_sec_adapter GTest::GTest GTest::gmock GTest::Main)
add_test(NAME TboxDiagTests COMMAND TboxDiagTests)

# 添加测试
add_executable(test_real_sec_adapter
    tests/test_real_sec_adapter.cpp
)

target_link_libraries(test_real_sec_adapter
    real_sec_adapter
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
