#pragma once

#include "sec_interface.h"
#include "tbox/sec/client.h"
#include <memory>

namespace tbox {
namespace diag {

class SecIpcAdapter : public SecInterface {
public:
    explicit SecIpcAdapter(std::shared_ptr<sec::SecClient> client);
    ~SecIpcAdapter() override = default;

    bool get_seed(uint8_t level, std::vector<uint8_t>& seed) override;
    bool verify_key(uint8_t level, const std::vector<uint8_t>& key) override;
    bool is_available() const override;

    bool generate_key_pair() override;
    bool get_csr(std::vector<uint8_t>& csr_der) override;
    bool submit_csr() override;
    bool inject_certificate(const std::vector<uint8_t>& cert_der) override;

private:
    std::shared_ptr<sec::SecClient> client_;
};

} // namespace diag
} // namespace tbox