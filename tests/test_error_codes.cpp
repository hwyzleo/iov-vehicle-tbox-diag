#include <gtest/gtest.h>
#include "error_codes.h"

namespace tbox {
namespace diag {
namespace testing {

TEST(ErrorCodesTest, SuccessCode) {
    EXPECT_EQ(error_code_to_string(DiagErrorCode::SUCCESS), "SUCCESS");
    EXPECT_EQ(error_code_to_description(DiagErrorCode::SUCCESS), "操作成功");
}

TEST(ErrorCodesTest, TransportConnectionFailed) {
    EXPECT_EQ(error_code_to_string(DiagErrorCode::TRANSPORT_CONNECTION_FAILED), "DIAG-1001");
    EXPECT_EQ(error_code_to_description(DiagErrorCode::TRANSPORT_CONNECTION_FAILED), "传输层连接/会话建立失败");
}

TEST(ErrorCodesTest, SessionStateNotAllowed) {
    EXPECT_EQ(error_code_to_string(DiagErrorCode::SESSION_STATE_NOT_ALLOWED), "DIAG-1002");
    EXPECT_EQ(error_code_to_description(DiagErrorCode::SESSION_STATE_NOT_ALLOWED), "会话状态不允许该请求");
}

TEST(ErrorCodesTest, SecurityAccessDenied) {
    EXPECT_EQ(error_code_to_string(DiagErrorCode::SECURITY_ACCESS_DENIED), "DIAG-1003");
    EXPECT_EQ(error_code_to_description(DiagErrorCode::SECURITY_ACCESS_DENIED), "安全访问未解锁/鉴权失败");
}

TEST(ErrorCodesTest, InvalidRequestFormat) {
    EXPECT_EQ(error_code_to_string(DiagErrorCode::INVALID_REQUEST_FORMAT), "DIAG-1004");
    EXPECT_EQ(error_code_to_description(DiagErrorCode::INVALID_REQUEST_FORMAT), "请求格式/参数非法");
}

TEST(ErrorCodesTest, DownstreamExecutionFailed) {
    EXPECT_EQ(error_code_to_string(DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED), "DIAG-1005");
    EXPECT_EQ(error_code_to_description(DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED), "下游服务(PROV)执行失败");
}

TEST(ErrorCodesTest, RequestTimeout) {
    EXPECT_EQ(error_code_to_string(DiagErrorCode::REQUEST_TIMEOUT), "DIAG-1006");
    EXPECT_EQ(error_code_to_description(DiagErrorCode::REQUEST_TIMEOUT), "请求处理超时");
}

TEST(ErrorCodesTest, InternalErrors) {
    EXPECT_EQ(error_code_to_string(DiagErrorCode::INTERNAL_ERROR), "INTERNAL-9999");
    EXPECT_EQ(error_code_to_string(DiagErrorCode::NOT_INITIALIZED), "INTERNAL-9998");
    EXPECT_EQ(error_code_to_string(DiagErrorCode::ROUTING_NOT_FOUND), "INTERNAL-9997");
    EXPECT_EQ(error_code_to_string(DiagErrorCode::SEC_UNAVAILABLE), "INTERNAL-9996");
    EXPECT_EQ(error_code_to_string(DiagErrorCode::PROV_UNAVAILABLE), "INTERNAL-9995");
}

TEST(ErrorCodesTest, UnknownCode) {
    EXPECT_EQ(error_code_to_string(static_cast<DiagErrorCode>(99999)), "UNKNOWN");
    EXPECT_EQ(error_code_to_description(static_cast<DiagErrorCode>(99999)), "未知错误");
}

TEST(ErrorCodesTest, NrcToDiagError) {
    EXPECT_EQ(nrc_to_diag_error(0x22), DiagErrorCode::SESSION_STATE_NOT_ALLOWED);
    EXPECT_EQ(nrc_to_diag_error(0x21), DiagErrorCode::SESSION_STATE_NOT_ALLOWED);
    EXPECT_EQ(nrc_to_diag_error(0x33), DiagErrorCode::SECURITY_ACCESS_DENIED);
    EXPECT_EQ(nrc_to_diag_error(0x35), DiagErrorCode::SECURITY_ACCESS_DENIED);
    EXPECT_EQ(nrc_to_diag_error(0x13), DiagErrorCode::INVALID_REQUEST_FORMAT);
    EXPECT_EQ(nrc_to_diag_error(0x31), DiagErrorCode::INVALID_REQUEST_FORMAT);
    EXPECT_EQ(nrc_to_diag_error(0x72), DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED);
    EXPECT_EQ(nrc_to_diag_error(0x78), DiagErrorCode::REQUEST_TIMEOUT);
}

} // namespace testing
} // namespace diag
} // namespace tbox
