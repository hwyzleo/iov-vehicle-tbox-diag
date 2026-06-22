#include <gtest/gtest.h>
#include "nrc_mapper.h"
#include "constants.h"

namespace tbox {
namespace diag {
namespace testing {

TEST(NrcMapperTest, DiagErrorToNrc) {
    EXPECT_EQ(NrcMapper::diag_error_to_nrc(DiagErrorCode::TRANSPORT_CONNECTION_FAILED), Nrc::CONDITIONS_NOT_CORRECT);
    EXPECT_EQ(NrcMapper::diag_error_to_nrc(DiagErrorCode::SESSION_STATE_NOT_ALLOWED), Nrc::CONDITIONS_NOT_CORRECT);
    EXPECT_EQ(NrcMapper::diag_error_to_nrc(DiagErrorCode::SECURITY_ACCESS_DENIED), Nrc::SECURITY_ACCESS_DENIED);
    EXPECT_EQ(NrcMapper::diag_error_to_nrc(DiagErrorCode::INVALID_REQUEST_FORMAT), Nrc::REQUEST_OUT_OF_RANGE);
    EXPECT_EQ(NrcMapper::diag_error_to_nrc(DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED), Nrc::GENERAL_PROGRAMMING_FAILURE);
    EXPECT_EQ(NrcMapper::diag_error_to_nrc(DiagErrorCode::REQUEST_TIMEOUT), Nrc::RESPONSE_PENDING);
    EXPECT_EQ(NrcMapper::diag_error_to_nrc(DiagErrorCode::INTERNAL_ERROR), Nrc::GENERAL_REJECT);
}

TEST(NrcMapperTest, ProvErrorToDiag) {
    EXPECT_EQ(NrcMapper::prov_error_to_diag(1001), DiagErrorCode::SECURITY_ACCESS_DENIED);
    EXPECT_EQ(NrcMapper::prov_error_to_diag(1002), DiagErrorCode::INVALID_REQUEST_FORMAT);
    EXPECT_EQ(NrcMapper::prov_error_to_diag(1003), DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED);
    EXPECT_EQ(NrcMapper::prov_error_to_diag(1004), DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED);
    EXPECT_EQ(NrcMapper::prov_error_to_diag(1005), DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED);
    EXPECT_EQ(NrcMapper::prov_error_to_diag(1006), DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED);
    EXPECT_EQ(NrcMapper::prov_error_to_diag(1007), DiagErrorCode::DOWNSTREAM_EXECUTION_FAILED);
    EXPECT_EQ(NrcMapper::prov_error_to_diag(9999), DiagErrorCode::INTERNAL_ERROR);
}

TEST(NrcMapperTest, NrcDescriptions) {
    EXPECT_EQ(NrcMapper::get_nrc_description(Nrc::GENERAL_REJECT), "generalReject");
    EXPECT_EQ(NrcMapper::get_nrc_description(Nrc::SERVICE_NOT_SUPPORTED), "serviceNotSupported");
    EXPECT_EQ(NrcMapper::get_nrc_description(Nrc::SUB_FUNCTION_NOT_SUPPORTED), "subFunctionNotSupported");
    EXPECT_EQ(NrcMapper::get_nrc_description(Nrc::INCORRECT_MESSAGE_LENGTH), "incorrectMessageLength");
    EXPECT_EQ(NrcMapper::get_nrc_description(Nrc::CONDITIONS_NOT_CORRECT), "conditionsNotCorrect");
    EXPECT_EQ(NrcMapper::get_nrc_description(Nrc::SECURITY_ACCESS_DENIED), "securityAccessDenied");
    EXPECT_EQ(NrcMapper::get_nrc_description(Nrc::INVALID_KEY), "invalidKey");
    EXPECT_EQ(NrcMapper::get_nrc_description(Nrc::GENERAL_PROGRAMMING_FAILURE), "generalProgrammingFailure");
    EXPECT_EQ(NrcMapper::get_nrc_description(Nrc::RESPONSE_PENDING), "responsePending");
}

TEST(NrcMapperTest, UnknownNrcDescription) {
    EXPECT_EQ(NrcMapper::get_nrc_description(0xFF), "unknownNRC");
}

} // namespace testing
} // namespace diag
} // namespace tbox
