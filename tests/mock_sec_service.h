#pragma once

#include "sec_service.h"
#include <gmock/gmock.h>

namespace tbox {
namespace sec {

class MockSecService : public SecService {
public:
    MOCK_METHOD(ErrorCode, initialize, (), (override));
    MOCK_METHOD(ErrorCode, generate_key_pair, (), (override));
    MOCK_METHOD(ErrorCode, get_csr, (std::vector<uint8_t>& csr_der), (override));
    MOCK_METHOD(ErrorCode, submit_csr, (), (override));
    MOCK_METHOD(ErrorCode, inject_certificate, (const std::vector<uint8_t>& cert_der), (override));
    MOCK_METHOD(ErrorCode, get_seed, (uint8_t level, std::vector<uint8_t>& seed), (override));
    MOCK_METHOD(ErrorCode, verify_key, (uint8_t level, const std::vector<uint8_t>& key), (override));
    MOCK_METHOD(ProvisionStatus, get_provision_status, (), (const, override));
    MOCK_METHOD(ErrorCode, reset_provision_status, (), (override));
    MOCK_METHOD(std::string, get_device_info, (), (const, override));
    MOCK_METHOD(bool, is_initialized, (), (const, override));
};

} // namespace sec
} // namespace tbox
