#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace tbox {
namespace diag {

class SecInterface {
public:
    virtual ~SecInterface() = default;

    virtual bool get_seed(uint8_t level, std::vector<uint8_t>& seed) = 0;
    virtual bool verify_key(uint8_t level, const std::vector<uint8_t>& key) = 0;
    virtual bool is_available() const = 0;
};

} // namespace diag
} // namespace tbox
