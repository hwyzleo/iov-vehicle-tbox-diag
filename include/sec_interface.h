#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace tbox {
namespace sec {
enum class ErrorCode : uint32_t;
}
namespace diag {

class SecInterface {
public:
    virtual ~SecInterface() = default;

    // 现有方法
    virtual bool get_seed(uint8_t level, std::vector<uint8_t>& seed) = 0;
    virtual bool verify_key(uint8_t level, const std::vector<uint8_t>& key) = 0;
    virtual bool is_available() const = 0;

    // 新增方法
    virtual bool generate_key_pair() = 0;
    virtual bool get_csr(std::vector<uint8_t>& csr_der) = 0;
    virtual bool submit_csr() = 0;
    virtual bool inject_certificate(const std::vector<uint8_t>& cert_der) = 0;
};

} // namespace diag
} // namespace tbox
