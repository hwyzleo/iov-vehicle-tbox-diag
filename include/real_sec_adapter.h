#pragma once

#include "sec_interface.h"
#include "sec_service.h"
#include <memory>

namespace tbox {
namespace diag {

class RealSecAdapter : public SecInterface {
public:
    explicit RealSecAdapter(std::shared_ptr<sec::SecService> service);
    ~RealSecAdapter() override = default;

    // 现有方法
    bool get_seed(uint8_t level, std::vector<uint8_t>& seed) override;
    bool verify_key(uint8_t level, const std::vector<uint8_t>& key) override;
    bool is_available() const override;

    // 新增方法
    bool generate_key_pair() override;
    bool get_csr(std::vector<uint8_t>& csr_der) override;
    bool submit_csr() override;
    bool inject_certificate(const std::vector<uint8_t>& cert_der) override;

private:
    std::shared_ptr<sec::SecService> service_;
};

} // namespace diag
} // namespace tbox
