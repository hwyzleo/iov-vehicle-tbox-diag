#pragma once

#include "sec_interface.h"
#include <vector>
#include <cstdint>

namespace tbox {
namespace diag {

class StubSecInterface : public SecInterface {
public:
    StubSecInterface() = default;
    ~StubSecInterface() override = default;

    bool get_seed(uint8_t level, std::vector<uint8_t>& seed) override {
        seed = {0x12, 0x34, 0x56, 0x78};
        return true;
    }

    bool verify_key(uint8_t level, const std::vector<uint8_t>& key) override {
        return true;
    }

    bool is_available() const override {
        return true;
    }
};

} // namespace diag
} // namespace tbox
