#include <gtest/gtest.h>
#include "diag_retry_policy.h"
#include "diag_ipc_protocol.h"

using namespace tbox::diag;

TEST(DiagRetryPolicyTest, KnownMethodsAreReadOnly) {
    EXPECT_EQ(DiagRetryPolicy::categorize(
        static_cast<uint32_t>(ipc::MethodId::GET_SERVICE_STATUS)),
        DiagRetryPolicy::Category::kReadOnly);
    EXPECT_EQ(DiagRetryPolicy::categorize(
        static_cast<uint32_t>(ipc::MethodId::GET_VEHICLE_INFO)),
        DiagRetryPolicy::Category::kReadOnly);
    EXPECT_EQ(DiagRetryPolicy::categorize(
        static_cast<uint32_t>(ipc::MethodId::IS_TESTER_CONNECTED)),
        DiagRetryPolicy::Category::kReadOnly);
}

TEST(DiagRetryPolicyTest, KnownMethodsAllowRetry) {
    EXPECT_TRUE(DiagRetryPolicy::should_retry(
        static_cast<uint32_t>(ipc::MethodId::GET_SERVICE_STATUS)));
    EXPECT_TRUE(DiagRetryPolicy::should_retry(
        static_cast<uint32_t>(ipc::MethodId::GET_VEHICLE_INFO)));
    EXPECT_TRUE(DiagRetryPolicy::should_retry(
        static_cast<uint32_t>(ipc::MethodId::IS_TESTER_CONNECTED)));
}

TEST(DiagRetryPolicyTest, UnknownMethodIsOneShot) {
    EXPECT_EQ(DiagRetryPolicy::categorize(9999), DiagRetryPolicy::Category::kOneShot);
    EXPECT_FALSE(DiagRetryPolicy::should_retry(9999));
}

TEST(DiagRetryPolicyTest, ZeroMethodIsOneShot) {
    EXPECT_EQ(DiagRetryPolicy::categorize(0), DiagRetryPolicy::Category::kOneShot);
    EXPECT_FALSE(DiagRetryPolicy::should_retry(0));
}
